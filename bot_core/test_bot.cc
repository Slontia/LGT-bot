#include "utility/msg_sender.h"
#include "bot_core/bot_core.h"
#include <gtest/gtest.h>
#include <gflags/gflags.h>

DEFINE_string(game_path, "plugins", "The path of game modules");
DEFINE_string(game_name, "猜拳游戏", "The path of game modules (max player should be 2)");

static std::ostream& operator<<(std::ostream& os, const ErrCode e) { return os << errcode2str(e); }

constexpr const uint64_t k_this_qq = 114514;
constexpr const uint64_t k_admin_qq = 1919810;
static int g_argc = 0;
static char** g_argv = nullptr;

typedef bool(*InitBotFunc)(const UserID, const NEW_MSG_SENDER_CALLBACK, const DELETE_MSG_SENDER_CALLBACK, int argc, char** argv);
typedef ErrCode(*HandlePrivateRequestFunc)(const UserID uid, const char* const msg);
typedef ErrCode(*HandlePublicRequestFunc)(const UserID uid, const GroupID gid, const char* const msg);

class MsgSenderForBotImpl : public MsgSenderForBot
{
 public:
  MsgSenderForBotImpl(const Target target, const uint64_t id) : target_(target), id_(id) {}

  virtual ~MsgSenderForBotImpl()
  {
    if (target_ == TO_USER)
    {
      std::cout << "[BOT -> USER_" << id_ << "]" << std::endl << ss_.str() << std::endl;
    }
    else if (target_ == TO_GROUP)
    {
      std::cout << "[BOT -> GROUP_" << id_ << "]" << std::endl << ss_.str() << std::endl;
    }
  }

  virtual void String(const char* const str, const size_t len) override
  {
    ss_ << std::string_view(str, len);
  }

  virtual void GroupUserName(const uint64_t uid, const uint64_t gid) override
  {
    ss_ << uid << "(" << gid << ")";
  }

  virtual void UserName(const uint64_t uid) override
  {
    ss_ << uid;
  }

  virtual void AtUser(const uint64_t uid) override
  {
    ss_ << "@" << uid;
  }

 private:
  const Target target_;
  const uint64_t id_;
  std::stringstream ss_;
};

MsgSenderForBot* create_msg_sender(const Target target, const uint64_t id)
{
  return new MsgSenderForBotImpl(target, id);
}

void delete_msg_sender(MsgSenderForBot* const msg_sender)
{
  delete msg_sender;
}
class TestBot : public testing::Test
{
 public:
  virtual void SetUp()
  {
    bot_ = BOT_API::Init(k_this_qq, create_msg_sender, delete_msg_sender, FLAGS_game_path.c_str(), &k_admin_qq, 1);
    ASSERT_NE(bot_, nullptr) << "init bot failed";
  }

  virtual void SetDown()
  {
    BOT_API::Release(bot_);
  }

   void* bot_;
};

#define ASSERT_PUB_MSG(ret, gid, uid, msg)\
do\
{\
  std::cout << "[USER_" << uid <<  " -> GROUP_" << gid << "]" << std::endl << msg << std::endl;\
  ASSERT_EQ((ret), BOT_API::HandlePublicRequest(bot_, (gid), (uid), (msg)));\
} while (0)

#define ASSERT_PRI_MSG(ret, uid, msg)\
do\
{\
  std::cout << "[USER_" << uid <<  " -> BOT]" << std::endl << msg << std::endl;\
  ASSERT_EQ((ret), BOT_API::HandlePrivateRequest(bot_, (uid), (msg)));\
} while (0)

// Join Not Existing Game

TEST_F(TestBot, pub_join_game_failed)
{
  ASSERT_PUB_MSG(EC_MATCH_GROUP_NOT_IN_MATCH, 1, 1, "#加入游戏");
}

TEST_F(TestBot, pub_join_pri_game_failed)
{
  ASSERT_PUB_MSG(EC_MATCH_NEED_REQUEST_PRIVATE, 1, 1, "#加入游戏 1");
}

TEST_F(TestBot, pri_join_game_failed)
{
  ASSERT_PRI_MSG(EC_MATCH_NOT_EXIST, 1, "#加入游戏 1");
}

TEST_F(TestBot, pri_join_pub_game_failed)
{
  ASSERT_PRI_MSG(EC_MATCH_NEED_ID, 1, "#加入游戏");
}

// Repeat New Game

TEST_F(TestBot, pub_repeat_new_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, pub_repeat_new_game_other_group)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 2, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, pub_repeat_new_pri_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, pri_repeat_new_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, pri_repeat_new_pub_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

// Join Self Game

TEST_F(TestBot, pub_join_self_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, 1, "#加入游戏");
}

TEST_F(TestBot, pri_join_self_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_USER_ALREADY_IN_MATCH, 1, "#加入游戏 1");
}

// Join Other Game After Creating Game

TEST_F(TestBot, pub_join_other_pub_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 2, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_OTHER_MATCH, 2, 1, "#加入游戏");
}

TEST_F(TestBot, pub_join_other_pri_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_USER_ALREADY_IN_OTHER_MATCH, 1, "#加入游戏 2");
}

TEST_F(TestBot, pri_join_other_pub_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 2, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_USER_ALREADY_IN_OTHER_MATCH, 2, 1, "#加入游戏");
}

TEST_F(TestBot, pri_join_other_pri_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_USER_ALREADY_IN_OTHER_MATCH, 1, "#加入游戏 2");
}

// Start Game Failed

TEST_F(TestBot, pub_start_game_not_host)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_NOT_HOST, 1, 2, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
}

TEST_F(TestBot, pri_start_game_not_host)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_MATCH_NOT_HOST, 2, "#开始游戏");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
}

TEST_F(TestBot, pub_start_game_not_in_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_USER_NOT_IN_MATCH, 1, 3, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
}

TEST_F(TestBot, pri_start_game_not_in_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_MATCH_USER_NOT_IN_MATCH, 3, "#开始游戏");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
}

TEST_F(TestBot, pub_start_other_pub_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 2, 3, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 2, 4, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_NOT_THIS_GROUP, 2, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 2, 3, "#开始游戏");
}

TEST_F(TestBot, pri_start_other_pub_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PUB_MSG(EC_OK, 2, 3, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 2, 4, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_NOT_THIS_GROUP, 2, 1, "#开始游戏");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 2, 3, "#开始游戏");
}

TEST_F(TestBot, pub_game_pri_start)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
}

// Exit Not Existing Game

TEST_F(TestBot, pub_exit_not_exist_game)
{
  ASSERT_PUB_MSG(EC_MATCH_USER_NOT_IN_MATCH, 1, 1, "#退出游戏");
}

TEST_F(TestBot, pri_exit_not_exist_game)
{
  ASSERT_PRI_MSG(EC_MATCH_USER_NOT_IN_MATCH, 1, "#退出游戏");
}

// Exit And New Game

TEST_F(TestBot, exit_pub_game_then_new_pub_game_same_group)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, exit_pub_game_then_new_pub_game_other_group)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 2, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_GROUP_NOT_IN_MATCH, 1, 2, "#加入游戏");
}

TEST_F(TestBot, exit_pri_game_then_new_pub_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_NOT_EXIST, 2, "#加入游戏 1");
}

TEST_F(TestBot, exit_pri_game_then_new_pri_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#退出游戏");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_NOT_EXIST, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 2");
}

// Exit And Join Game

TEST_F(TestBot, exit_pub_game_then_join_pub_game_same_group)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#加入游戏");
}

TEST_F(TestBot, exit_pub_game_then_join_pub_game_other_group)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 2, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 2, 1, "#加入游戏");
}

TEST_F(TestBot, exit_pri_game_then_join_pub_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#加入游戏");
}

TEST_F(TestBot, exit_pri_game_then_join_pri_game)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#退出游戏");
  ASSERT_PRI_MSG(EC_MATCH_NOT_EXIST, 1, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, "#加入游戏 2");
}

// Switch Host

TEST_F(TestBot, switch_host)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 3, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_USER_NOT_IN_MATCH, 1, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_MATCH_NOT_HOST, 1, 3, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#开始游戏");
}

// Config Game

TEST_F(TestBot, config_game)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#配置新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_MATCH_IN_CONFIG, 1, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_MATCH_IN_CONFIG, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_USER_NOT_IN_MATCH, 1, 2, "#配置完成");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#配置完成");
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
}

TEST_F(TestBot, config_game_exit)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#配置新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#退出游戏");
  ASSERT_PUB_MSG(EC_MATCH_GROUP_NOT_IN_MATCH, 1, 2, "#加入游戏");
}

// Player Limit

TEST_F(TestBot, exceed_max_player)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_MATCH_ACHIEVE_MAX_PLAYER, 1, 3, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
}

// Administor

TEST_F(TestBot, interrupt_private_without_mid)
{
  ASSERT_PRI_MSG(EC_MATCH_NEED_REQUEST_PUBLIC, k_admin_qq, "%中断游戏");
}

TEST_F(TestBot, interrupt_public_not_game)
{
  ASSERT_PUB_MSG(EC_MATCH_GROUP_NOT_IN_MATCH, 1, k_admin_qq, "%中断游戏");
}

TEST_F(TestBot, interrupt_public_config)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#配置新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, k_admin_qq, "%中断游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_public_wait)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, k_admin_qq, "%中断游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_public_start)
{
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 1, 2, "#加入游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 1, k_admin_qq, "%中断游戏");
  ASSERT_PUB_MSG(EC_OK, 1, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_wait)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#配置新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_config)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_OK, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_start)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
  ASSERT_PRI_MSG(EC_OK, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_wait_in_public)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#配置新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PUB_MSG(EC_OK, 999, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_config_in_public)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PUB_MSG(EC_OK, 999, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

TEST_F(TestBot, interrupt_private_start_in_public)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_OK, 2, "#加入游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
  ASSERT_PUB_MSG(EC_OK, 999, k_admin_qq, "%中断游戏 1");
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
}

// Computer Player

TEST_F(TestBot, set_computer)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name + " 电脑").c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#电脑数量 1");
  ASSERT_PRI_MSG(EC_MATCH_ACHIEVE_MAX_PLAYER, 2, ("#加入游戏 1"));
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
}

TEST_F(TestBot, set_computer_config)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#配置新游戏 " + FLAGS_game_name + " 电脑").c_str());
  ASSERT_PRI_MSG(EC_OK, 1, "#电脑数量 1");
  ASSERT_PRI_MSG(EC_OK, 1, "#配置完成");
  ASSERT_PRI_MSG(EC_MATCH_ACHIEVE_MAX_PLAYER, 2, ("#加入游戏 1"));
  ASSERT_PRI_MSG(EC_OK, 1, "#开始游戏");
}

TEST_F(TestBot, not_allow_set_computer)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name).c_str());
  ASSERT_PRI_MSG(EC_MATCH_FORBID_OPERATION, 1, "#电脑数量 1");
}

TEST_F(TestBot, computer_exceed_max_player)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#新游戏 " + FLAGS_game_name + " 电脑").c_str());
  ASSERT_PRI_MSG(EC_MATCH_ACHIEVE_MAX_PLAYER, 1, "#电脑数量 2");
  ASSERT_PRI_MSG(EC_OK, 2, ("#加入游戏 1"));
}

TEST_F(TestBot, computer_exceed_max_player_when_config)
{
  ASSERT_PRI_MSG(EC_OK, 1, ("#配置新游戏 " + FLAGS_game_name + " 电脑").c_str());
  ASSERT_PRI_MSG(EC_MATCH_ACHIEVE_MAX_PLAYER, 1, "#电脑数量 2");
  ASSERT_PRI_MSG(EC_OK, 1, ("#配置完成"));
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  g_argc = argc;
  g_argv = argv;
  return RUN_ALL_TESTS();
}
