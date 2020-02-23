#include "game_stage.h"
#include "msg_checker.h"
#include "dllmain.h"
#include "resource.h"
#include <memory>
#include <array>
#include <functional>
#include "resource_loader.h"

const std::string k_game_name = "LIE";
const uint64_t k_min_player = 2; /* should be larger than 1 */
const uint64_t k_max_player = 2; /* 0 means no max-player limits */
const char* Rule()
{
  static std::string rule = LoadText(IDR_TEXT1_RULE, TEXT("Text"));
  return rule.c_str();
}

class NumberStage : public AtomStage
{
 public:
  NumberStage(const uint64_t questioner)
    : AtomStage("�������ֽ׶�",
      {
        MakeStageCommand(this, "��������", &NumberStage::Number, std::make_unique<ArithChecker<int, 1, 6>>("����")),
      }), questioner_(questioner), num_(0) {}

  int num() const { return num_; }

 private:
   bool Number(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const int num)
   {
     if (pid != questioner_)
     {
       reply("[����] ���غ���Ϊ�²��ߣ��޷���������");
       return false;
     }
     if (is_public)
     {
       reply("[����] ��˽�Ų���ѡ�����֣�����ѡ����Ч");
       return false;
     }
     num_ = num;
     reply("���óɹ�������������");
     return true;
   }

   const uint64_t questioner_;
   int num_;
};

class LieStage : public AtomStage
{
public:
  LieStage(const uint64_t questioner)
    : AtomStage("�������ֽ׶�",
      {
        MakeStageCommand(this, "��������", &LieStage::Lie, std::make_unique<ArithChecker<int, 1, 6>>("����")),
      }), questioner_(questioner), lie_num_(0) {}

  int lie_num() const { return lie_num_; }

private:
  bool Lie(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const int lie_num)
  {
    if (pid != questioner_)
    {
      reply("[����] ���غ���Ϊ�²��ߣ��޷�����");
      return false;
    }
    lie_num_ = lie_num;
    Boardcast((std::stringstream() << "���" << At(pid) << "��������" << lie_num << "�������" << At(1 - pid) << "���Ż�����").str());
    return true;
  }

  const uint64_t questioner_;
  int lie_num_;
};

class GuessStage : public AtomStage
{
public:
  GuessStage(const uint64_t guesser)
    : AtomStage("�������ֽ׶�",
      {
        MakeStageCommand(this, "�²�", &GuessStage::Guess, std::make_unique<BoolChecker>("����", "����")),
      }), guesser_(guesser) {}

  bool doubt() const { return doubt_; }

private:
  bool Guess(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const bool doubt)
  {
    if (pid != guesser_)
    {
      reply("[����] ���غ���Ϊ�����ߣ��޷��²�");
      return false;
    }
    doubt_ = doubt;
    return true;
  }

  const uint64_t guesser_;
  bool doubt_;
};

class RoundStage : public CompStage<NumberStage, LieStage, GuessStage>
{
 public:
   RoundStage(const uint64_t round, const uint64_t questioner, std::array<std::array<int, 6>, 2>& player_nums)
     : CompStage("��" + std::to_string(round) + "�غ�", {}),
     questioner_(questioner), num_(0), lie_num_(0), player_nums_(player_nums), loser_(0) {}

   uint64_t loser() const { return loser_; }

   virtual VariantSubStage OnStageBegin() override
   {
     Boardcast(name_ + "��ʼ�������" + At(questioner_) + "˽�Ų���ѡ������");
     return std::make_unique<NumberStage>(questioner_);
   }

   virtual VariantSubStage NextSubStage(NumberStage& sub_stage, const bool is_timeout) override
   {
     num_ = sub_stage.num();
     return std::make_unique<LieStage>(questioner_);
   }

   virtual VariantSubStage NextSubStage(LieStage& sub_stage, const bool is_timeout) override
   {
     lie_num_ = sub_stage.lie_num();
     return std::make_unique<GuessStage>(1 - questioner_);
   }

   virtual VariantSubStage NextSubStage(GuessStage& sub_stage, const bool is_timeout) override
   {
     const bool doubt = sub_stage.doubt();
     const bool suc = doubt ^ (num_ == lie_num_);
     loser_ = suc ? questioner_ : 1 - questioner_;
     ++player_nums_[loser_][num_ - 1];
     std::stringstream ss;
     ss << "ʵ������Ϊ" << num_ << "��"
       << (doubt ? "����" : "����") << (suc ? "�ɹ�" : "ʧ��") << "��"
       << "���" << At(loser_) << "�������" << num_ << std::endl
       << "���ֻ�������" << std::endl << At(0) << "��" << At(1);
     for (int num = 1; num <= 6; ++num)
     {
       ss << std::endl << player_nums_[0][num - 1] << " [" << num << "] " << player_nums_[1][num - 1];
     }
     Boardcast(ss.str());
     return {};
   }

private:
  const uint64_t questioner_;
  int num_;
  int lie_num_;
  std::array<std::array<int, 6>, 2>& player_nums_;
  uint64_t loser_;
};

class MainStage : public CompStage<RoundStage>
{
 public:
  MainStage() : CompStage("", {}), questioner_(0), round_(1), player_nums_{ {0} } {}

  virtual VariantSubStage OnStageBegin() override
  {
    return std::make_unique<RoundStage>(1, std::rand() % 2, player_nums_);
  }

  virtual VariantSubStage NextSubStage(RoundStage& sub_stage, const bool is_timeout) override
  {
    questioner_ = sub_stage.loser();
    if (JudgeOver()) { return {}; }
    return std::make_unique<RoundStage>(++round_, questioner_, player_nums_);
  }

  int64_t PlayerScore(const uint64_t pid) const
  {
    return pid == questioner_ ? -10 : 10;
  }

 private:
   bool JudgeOver()
   {
     bool has_all_num = true;
     for (const int count : player_nums_[questioner_])
     {
       if (count >= 3) { return true; }
       else if (count == 0) { has_all_num = false; }
     }
     return has_all_num;
   }

   uint64_t questioner_;
   uint64_t round_;
   std::array<std::array<int, 6>, 2> player_nums_;
};

std::pair<std::unique_ptr<Stage>, std::function<int64_t(uint64_t)>> MakeMainStage(const uint64_t player_num)
{
  assert(player_num == 2);
  std::unique_ptr<MainStage> main_stage = std::make_unique<MainStage>();
  const auto get_player_score = std::bind(&MainStage::PlayerScore, main_stage.get(), std::placeholders::_1);
  return { static_cast<std::unique_ptr<Stage>&&>(std::move(main_stage)), get_player_score };
}
