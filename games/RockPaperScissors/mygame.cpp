#include "mygame.h"
#include "game_stage.h"
#include "msg_checker.h"
#include "dllmain.h"
#include "resource.h"
#include <memory>
#include <array>
#include <functional>
#include "resource_loader.h"

const std::string k_game_name = "��ȭ��Ϸ";
const uint64_t k_min_player = 2; /* should be larger than 1 */
const uint64_t k_max_player = 2; /* 0 means no max-player limits */
const char* Rule()
{
  static std::string rule = LoadText(IDR_TEXT1_RULE, TEXT("Text"));
  return rule.c_str();
}

std::vector<int64_t> GameEnv::PlayerScores() const
{
  const auto score = [player_envs = player_envs_](const uint64_t pid)
  {
    return (player_envs[pid].win_count_ == 3) ? 10 : -10;
  };
  return {score(0), score(1)};
}

static std::string Choise2Str(const Choise& choise)
{
  return choise == SCISSORS_CHOISE ? "����" :
    choise == ROCK_CHOISE ? "ʯͷ" :
    choise == PAPER_CHOISE ? "��" :
    "δѡ��";
}

class RoundStage : public AtomStage<GameEnv>
{
 public:
   RoundStage(const uint64_t round, Game<GameEnv>& game)
     : AtomStage(game, "��" + std::to_string(round) + "�غ�",
       {
         MakeStageCommand(this, "��ȭ", &RoundStage::Act_, 
           std::make_unique<AlterChecker<Choise>>(std::map<std::string, Choise> {
             { "����", SCISSORS_CHOISE},
             { "ʯͷ", ROCK_CHOISE },
             { "��", PAPER_CHOISE }
           }, "ѡ��")),
       })
   {
     game_.game_env().player_envs_[0].cur_choise_ = NONE_CHOISE;
     game_.game_env().player_envs_[1].cur_choise_ = NONE_CHOISE;
     game_.Boardcast("��˽�Ų��н���ѡ��");
   }

private:
  bool Act_(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, Choise choise)
  {
    if (is_public)
    {
      reply("��˽�Ų���ѡ�񣬹���ѡ����Ч");
      return false;
    }
    Choise& cur_choise = game_.game_env().player_envs_[pid].cur_choise_;
    if (cur_choise != NONE_CHOISE)
    {
      reply("���Ѿ����й�ѡ����");
      return false;
    }
    cur_choise = choise;
    reply("ѡ��ɹ�");
    return JudgeWinner_();
  }

  bool JudgeWinner_()
  {
    const auto choise = [&game = game_](const uint64_t pid) { return game.game_env().player_envs_[pid].cur_choise_; };
    const auto win_count = [&game = game_](const uint64_t pid) { return game.game_env().player_envs_[pid].win_count_; };
    if (choise(0) == NONE_CHOISE || choise(1) == NONE_CHOISE) { return false; }
    std::stringstream ss;
    ss << "���" << game_.At(0) << "��" << Choise2Str(choise(0)) << std::endl;
    ss << "���" << game_.At(1) << "��" << Choise2Str(choise(1)) << std::endl;
    const auto is_win = [&game = game_](const uint64_t pid)
    {
      const Choise& my_choise = game.game_env().player_envs_[pid].cur_choise_;
      const Choise& oppo_choise = game.game_env().player_envs_[1 - pid].cur_choise_;
      return (my_choise == PAPER_CHOISE && oppo_choise == ROCK_CHOISE) ||
        (my_choise == ROCK_CHOISE && oppo_choise == SCISSORS_CHOISE) ||
        (my_choise == SCISSORS_CHOISE && oppo_choise == PAPER_CHOISE);
    };
    const auto on_win = [&game = game_, &ss](const uint64_t pid)
    {
      ss << "���" << game.At(pid) << "ʤ��" << std::endl;
      ++game.game_env().player_envs_[pid].win_count_;
    };
    if (is_win(0)) { on_win(0); }
    else if (is_win(1)) { on_win(1); }
    else { ss << "ƽ��" << std::endl; }
    ss << "Ŀǰ�ȷ֣�" << std::endl;
    ss << game_.At(0) << " " << game_.game_env().player_envs_[0].win_count_ << " - " <<
      game_.game_env().player_envs_[1].win_count_ << " " << game_.At(1);
    game_.Boardcast(ss.str());
    return true;
  }
};

class MainStage : public CompStage<GameEnv, RoundStage>
{
 public:
  MainStage(Game<GameEnv>& game)
    : CompStage(game, "", {}, std::make_unique<RoundStage>(1, game)), round_(1) {}

  virtual VariantSubStage NextSubStage(RoundStage&) override
  {
    GameEnv& env = game_.game_env();
    if (env.player_envs_[0].win_count_ == 3 || env.player_envs_[1].win_count_ == 3) { return {}; }
    return std::make_unique<RoundStage>(++ round_, game_);
  }

 private:
   uint64_t round_;
};

std::unique_ptr<GameEnv> MakeGameEnv(const uint64_t player_num)
{
  assert(player_num == 2);
  return std::make_unique<GameEnv>();
}

std::unique_ptr<Stage<GameEnv>> MakeMainStage(Game<GameEnv>& game)
{
  return std::make_unique<MainStage>(game);
}

GameBase* __cdecl NewGame(void* const match, const uint64_t player_num)
{
  if (player_num < k_min_player || (player_num > k_max_player && k_max_player != 0)) { return nullptr; }
  Game<GameEnv>* game = new Game<GameEnv>(match, MakeGameEnv(player_num));
  game->SetMainStage(MakeMainStage(*game));
  return game;
}
