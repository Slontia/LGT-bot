#include "stdafx.h"

#include <sstream>


#include "message_iterator.h"
#include "game_container.h"

#include "message_handlers.h"
#include "match.h"

static std::string read_gamename(MessageIterator& msg);

template <class T>
static bool to_type(const std::string& str, T& res)
{
  return (bool) (std::stringstream(str) >> res);
}

void show_gamelist(MessageIterator& msg)
{
  auto gamelist = game_container.gamelist();
  std::stringstream ss;
  int i = 0;
  ss << "��Ϸ�б�";
  for (auto it = gamelist.begin(); it != gamelist.end(); it++)
  {
    auto name = *it;
    ss << std::endl << i << '.' << name;
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
  if (!game_container.has_game(name))
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
    msg.Reply("δ֪����Ϸ��");
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
          msg.Reply("������Ϸ����Ⱥ���������н���");
          return;
        case GROUP_MSG:
          msg.Reply(match_manager.new_match(GROUP_MATCH, gamename, msg.usr_qq_, msg.src_qq_), "�����ɹ�");
          break;
        case DISCUSS_MSG:
          msg.Reply(match_manager.new_match(DISCUSS_MATCH, gamename, msg.usr_qq_, msg.src_qq_), "�����ɹ�");
          break;
        default:
          /* Never reach here! */
          assert(false);
      }
    }
    else if (match_type == "˽��")
    {
      msg.Reply(match_manager.new_match(PRIVATE_MATCH, gamename, msg.usr_qq_, INVALID_QQ), "�����ɹ�");
    }
    else
    {
      msg.Reply("δ֪�ı������ͣ�Ӧ��Ϊ��������˽��");
    }
  }
}



void start_game(MessageIterator& msg)
{
  if (msg.has_next())
  {
    msg.Reply("����Ĳ���");
    return;
  }
  msg.Reply(match_manager.StartGame(msg.usr_qq_), "��ʼ��Ϸ�ɹ�");
}

void leave(MessageIterator& msg)
{
  if (msg.has_next())
  {
    msg.Reply("����Ĳ���");
    return;
  }
  msg.Reply(match_manager.DeletePlayer(msg.usr_qq_), "�˳��ɹ�");
}

void join(MessageIterator& msg)
{
  MatchId id;
  if (msg.has_next())
  {
    /* Match ID */
    if (!to_type(msg.get_next(), id))
    {
      msg.Reply("����ID����Ϊ������");
      return;
    }
    if (msg.has_next())
    {
      msg.Reply("����Ĳ���");
      return;
    }
  }
  else
  {
    std::shared_ptr<Match> match = nullptr;
    switch (msg.type_)
    {
      case PRIVATE_MSG:
        msg.Reply("˽����Ϸ����ָ������id");
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
  msg.Reply(match_manager.AddPlayer(id, msg.usr_qq_), "����ɹ���");
}