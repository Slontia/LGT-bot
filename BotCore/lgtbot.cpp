#include "stdafx.h"
#include "my_game.h"
#include "lgtbot.h"
#include "message_iterator.h"
#include "message_handlers.h"

#include <list>
#include <sstream>
#include <memory>

#define CQ_CODE_LEN 256

char cq_code_buf[CQ_CODE_LEN];

#define RETURN_IF_FAILED(str) \
do\
{\
  if (const std::string& err = (str); !err.empty())\
    return err;\
} while (0);

GameContainer game_container;
MatchManager match_manager;

QQ LGTBOT::this_qq = INVALID_QQ;
AT_CALLBACK LGTBOT::at_cq = nullptr;
MSG_CALLBACK LGTBOT::send_private_msg_callback = nullptr;
MSG_CALLBACK LGTBOT::send_group_msg_callback = nullptr;
MSG_CALLBACK LGTBOT::send_discuss_msg_callback = nullptr;

std::string LGTBOT::at_str(const QQ& qq)
{
  at_cq(qq, cq_code_buf, CQ_CODE_LEN);
  return std::string(cq_code_buf);
}

void LGTBOT::set_this_qq(const QQ& qq)
{
  this_qq = qq;
}

void LGTBOT::set_at_cq_callback(AT_CALLBACK fun)
{
  at_cq = fun;
}

void LGTBOT::set_send_private_msg_callback(MSG_CALLBACK fun)
{
  send_private_msg_callback = fun;
}

void LGTBOT::set_send_group_msg_callback(MSG_CALLBACK fun)
{
  send_group_msg_callback = fun;
}

void LGTBOT::set_send_discuss_msg_callback(MSG_CALLBACK fun)
{
  send_discuss_msg_callback = fun;
}

bool LGTBOT::is_at(const std::string& msg)
{
  return msg.find(at_str(this_qq)) != std::string::npos;
}

void LGTBOT::send_private_msg(const QQ& usr_qq, const std::string& msg)
{
  send_private_msg_callback(usr_qq, msg.c_str());
}

void LGTBOT::send_group_msg(const QQ& group_qq, const std::string& msg)
{
  send_group_msg_callback(group_qq, msg.c_str());
}

void LGTBOT::send_discuss_msg(const QQ& discuss_qq, const std::string& msg)
{
  send_discuss_msg_callback(discuss_qq, msg.c_str());
}

void LGTBOT::send_group_msg(const QQ& group_qq, const std::string& msg, const QQ& to_usr_qq)
{
  send_group_msg(group_qq, at_str(to_usr_qq) + msg);
}

void LGTBOT::send_discuss_msg(const QQ& discuss_qq, const std::string& msg, const QQ& to_usr_qq)
{
  send_discuss_msg(discuss_qq, at_str(to_usr_qq) + msg);
}

void LGTBOT::LoadGames()
{
  LOG_INFO("��˹������������������ˣ�");
  Bind(game_container); /* Mygame */
}

/* msg is not empty */
void global_request(MessageIterator& msg)
{
  assert(msg.has_next());
  const std::string front_msg = msg.get_next();
  if (front_msg == "��Ϸ����") 
  {

  }
  else if (front_msg == "��Ϸ�б�")
  {
    show_gamelist(msg);
  }
  else if (front_msg == "����")
  {

  }
  else if (front_msg == "����Ϸ") // ����Ϸ <��Ϸ��> (����|˽��)
  {
    new_game(msg);
  }
  else if (front_msg == "����")
  {

  }
  else if (front_msg == "��ʼ") // host only
  {
    start_game(msg);
  }
  else if (front_msg == "����") // ���� [�������]
  {
    join(msg);
  }
  else if (front_msg == "�˳�")
  {
    leave(msg);
  }
}

bool LGTBOT::Request(const MessageType& msg_type, const QQ& src_qq, const QQ& usr_qq, char* msg_c)
{
  std::string msg(msg_c);
  if (msg.empty()) return false;

  bool is_global = false;
  size_t last_pos = 0, pos = 0;

  /* If it is a global request, we should move char pointer behind '#'. */
  if (msg[0] == '#')
  {
    last_pos = 1;
    is_global = true;
  }
  
  std::vector<std::string> substrs;
  while (true)
  {
    /* Read next word. */
    pos = msg.find(" ", last_pos);
    if (last_pos != pos) substrs.push_back(msg.substr(last_pos, pos));

    /* If finish reading, break. */
    if (pos == std::string::npos) break;
    last_pos = pos + 1;
  }
  if (substrs.empty()) return true;

  /* Make Message Iterator. */
  std::shared_ptr<MessageIterator> msgit_p = 
    (msg_type == PRIVATE_MSG) ? std::dynamic_pointer_cast<MessageIterator>(std::make_shared<PrivateMessageIterator>(usr_qq, std::move(substrs))) :
    (msg_type == GROUP_MSG)   ? std::dynamic_pointer_cast<MessageIterator>(std::make_shared<GroupMessageIterator>(usr_qq, std::move(substrs), src_qq)) :
    (msg_type == DISCUSS_MSG) ? std::dynamic_pointer_cast<MessageIterator>(std::make_shared<DiscussMessageIterator>(usr_qq, std::move(substrs), src_qq)) :
                                nullptr;
  assert(msgit_p != nullptr);
  MessageIterator& msgit = *msgit_p;
  /* Distribute msg to global handle or match handle. */
  if (is_global)
    global_request(msgit);
  else
    match_manager.Request(msgit);
  return true;
}
