#include "stdafx.h"

#include <sstream>
#include <tuple>

#include "message_iterator.h"
#include "message_handlers.h"
#include "match.h"
#include "lgtbot.h"

static std::string read_gamename(MessageIterator& msg);

template <class T>
static bool to_type(const std::string& str, T& res)
{
  return (bool) (std::stringstream(str) >> res);
}

void show_gamelist(MessageIterator& msg)
{
  std::stringstream ss;
  int i = 0;
  ss << "��Ϸ�б�";
  for (const auto& [name, game] : g_game_handles_)
  {
    ss << std::endl << (++ i) << '.' << name;
  }
  msg.Reply(ss.str());
}

std::string read_gamename(MessageIterator& msg)
{
  if (!msg.has_next())
  {
    /* error: without game name */
    return "";
  }
  const std::string& name = msg.get_next();
  const auto it = g_game_handles_.find(name);
  if (it == g_game_handles_.end())
  {
    /* error: game not found */
    return "";
  }
  return name;
};

void new_game(MessageIterator& msg)
{
  auto gamename = read_gamename(msg);
  if (gamename.empty())
  {
    msg.ReplyError("������Ϸʧ�ܣ�δ֪����Ϸ��");
    return;
  }
  if (msg.has_next())
  {
    auto match_type = msg.get_next();
    if (match_type == "����")
    {
      switch (msg.type_)
      {
        case PRIVATE_MSG:
          msg.ReplyError("������Ϸʧ�ܣ�����������Ϸ��֧��˽�Ž���������Ⱥ���������й�������");
          return;
        case GROUP_MSG:
          msg.Reply(match_manager.new_match(GROUP_MATCH, gamename, msg.usr_qq_, msg.src_qq_), "������Ϸ�ɹ���");
          break;
        case DISCUSS_MSG:
          msg.Reply(match_manager.new_match(DISCUSS_MATCH, gamename, msg.usr_qq_, msg.src_qq_), "������Ϸ�ɹ���");
          break;
        default:
          /* Never reach here! */
          assert(false);
      }
    }
    else if (match_type == "˽��")
    {
      msg.Reply(match_manager.new_match(PRIVATE_MATCH, gamename, msg.usr_qq_, INVALID_QQ), "������Ϸ�ɹ���");
    }
    else
    {
      msg.ReplyError("������Ϸʧ�ܣ�δ֪�ı������ͣ�Ӧ��Ϊ�����������ߡ�˽�ܡ�");
    }
  }
}

void start_game(MessageIterator& msg)
{
  if (msg.has_next())
  {
    msg.ReplyError("��ʼ��Ϸʧ�ܣ�����Ĳ���");
    return;
  }
  msg.Reply(match_manager.StartGame(msg.usr_qq_), "��Ϸ��ʼ��");
}

void leave(MessageIterator& msg)
{
  if (msg.has_next())
  {
    msg.ReplyError("�˳���Ϸʧ�ܣ�����Ĳ���");
    return;
  }
  msg.Reply(match_manager.DeletePlayer(msg.usr_qq_), "");
}

void join(MessageIterator& msg)
{
  MatchId id;
  if (msg.has_next())
  {
    /* Match ID */
    if (!to_type(msg.get_next(), id))
    {
      msg.ReplyError("����ʧ�ܣ�����ID����Ϊ����");
      return;
    }
    if (msg.has_next())
    {
      msg.ReplyError("����ʧ�ܣ�����Ĳ���");
      return;
    }
  }
  else
  {
    std::shared_ptr<Match> match = nullptr;
    switch (msg.type_)
    {
      case PRIVATE_MSG:
        msg.ReplyError("����ʧ�ܣ�˽��������Ϸ������ָ������ID");
        return;
      case GROUP_MSG:
        id = match_manager.get_group_match_id(msg.src_qq_);
        break;
      case DISCUSS_MSG:
        id = match_manager.get_discuss_match_id(msg.src_qq_);
        break;
      default:
        assert(false);
    }
  }
  msg.Reply(match_manager.AddPlayer(id, msg.usr_qq_), "");
}