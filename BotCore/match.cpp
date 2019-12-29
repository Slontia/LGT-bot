#include "stdafx.h"
#include "match.h"
#include "log.h"
#include "match.h"
#include "../new-rock-paper-scissors/dllmain.h"

MatchManager::MatchManager() : next_match_id(1) {}

std::shared_ptr<Match> MatchManager::new_private_match(const MatchId& id, const std::string& game_id, const QQ& host_qq)
{
  return std::dynamic_pointer_cast<Match>(std::make_shared<PrivateMatch>(id, game_id, host_qq));
}

std::shared_ptr<Match> MatchManager::new_group_match(const MatchId& id, const std::string& game_id, const QQ& host_qq, const QQ& group_qq)
{
  assert(id != INVALID_LOBBY);

  /* If the group has match, return. */
  if (group_matches.find(group_qq) != group_matches.end()) return nullptr;

  /* Create new match. */
  auto match = std::make_shared<GroupMatch>(id, game_id, host_qq, group_qq);

  group_matches.emplace(group_qq, match);
  return std::dynamic_pointer_cast<Match>(match);
}

std::shared_ptr<Match> MatchManager::new_discuss_match(const MatchId& id, const std::string& game_id, const QQ& host_qq, const QQ& discuss_qq)
{
  assert(id != INVALID_LOBBY);

  /* If the discuss has match, return. */
  if (discuss_matches.find(discuss_qq) != discuss_matches.end()) return nullptr;

  /* Create new match. */
  auto match = std::make_shared<DiscussMatch>(id, game_id, host_qq, discuss_qq);
  
  discuss_matches.emplace(discuss_qq, match);
  return std::dynamic_pointer_cast<Match>(match);
}

ErrMsg MatchManager::new_match(const MatchType& type, const std::string& game_id, const QQ& host_qq, const QQ& lobby_qq)
{
  if ((player_matches.find(host_qq) != player_matches.end())) return "���Ѽ�����Ϸ�������½���Ϸ";

  /* Create id and match. */
  auto id = assign_id();
  std::shared_ptr<Match> new_match = nullptr;
  switch (type)
  {
    case PRIVATE_MATCH:
      new_match = new_private_match(id, game_id, host_qq);
      assert(new_match);
      break;
    case GROUP_MATCH:
      new_match = new_group_match(id, game_id, host_qq, lobby_qq);
      break;
    case DISCUSS_MATCH:
      new_match = new_discuss_match(id, game_id, host_qq, lobby_qq);
      break;
    default:
      /* Never reach here! */
      assert(false);
  }
  if (!new_match) return "�÷����Ѿ���ʼ����Ϸ���������½���Ϸ��";

  /* Store match into map. */
  assert(matches.find(id) == matches.end());
  matches.emplace(id, new_match);

  /* Add host player. */
  RETURN_IF_FAILED(AddPlayer(id, host_qq));

  return "";
}

ErrMsg MatchManager::StartGame(const QQ& host_qq)
{
  auto it = player_matches.find(host_qq);
  if (it == player_matches.end()) return "��δ������Ϸ";

  auto match = it->second;
  if (match->get_host_qq() != host_qq) return "�����Ƿ�����û�п�ʼ��Ϸ��Ȩ��";
  RETURN_IF_FAILED(match->GameStart());
  return "";
}

// private
MatchId MatchManager::get_match_id(const QQ& src_qq, std::unordered_map<QQ, std::shared_ptr<Match>> match_map)
{
  auto it = match_map.find(src_qq);
  return (it == match_map.end()) ? INVALID_MATCH : it->second->get_id();
}

MatchId MatchManager::get_group_match_id(const QQ& group_qq)
{
  return get_match_id(group_qq, group_matches);
}

MatchId MatchManager::get_discuss_match_id(const QQ& discuss_qq)
{
  return get_match_id(discuss_qq, discuss_matches);
}


/* Assume usr_qq is valid. */
std::string MatchManager::AddPlayer(const MatchId& id, const QQ& usr_qq)
{
  auto it = matches.find(id);
  if (it == matches.end()) return "�÷���δ������Ϸ������ϷID������";
  if (player_matches.find(usr_qq) != player_matches.end()) return "���Ѽ���������Ϸ�������˳�";
  auto match = it->second;

  /* Join match. */
  RETURN_IF_FAILED(match->Join(usr_qq));
  
  /* Insert into player matches map. */
  player_matches.emplace(usr_qq, match);

  return "";
}

/* Assume usr_qq is valid. */
std::string MatchManager::DeletePlayer(const QQ& usr_qq)
{
  
  auto it = player_matches.find(usr_qq);
  if (it == player_matches.end()) return "��δ������Ϸ���˳�ʧ��";
  auto match = it->second;

  /* Leave match. */
  RETURN_IF_FAILED(match->Leave(usr_qq));

  /* Erase from player matches map. */
  player_matches.erase(usr_qq);

  /* If host quit, switch host. */
  if (usr_qq == match->get_host_qq() && !match->switch_host())
  {
    DeleteMatch(match->get_id());
  }

  return "";
}

/* return true if is a game request */
bool MatchManager::Request(MessageIterator& msg)
{
  switch (msg.type_)
  {
    case PRIVATE_MSG:
    {
      auto match = player_matches.find(msg.usr_qq_);
      if (match == player_matches.end()) return false;
      match->second->Request(msg);
      return true;
    }
    case GROUP_MSG:
      return PublicRequest(msg, group_matches);
    case DISCUSS_MSG:
      return PublicRequest(msg, discuss_matches);
    default:
      break;
  }
  /* Never reach here! */
  assert(false);
  return false;
}

void MatchManager::DeleteMatch(MatchId id)
{
  auto it = matches.find(id);
  assert(it != matches.end());
  auto match = it->second;
  /* Delete match from group or discuss. */
  switch (match->get_type())
  {
    case GROUP_MATCH:
      group_matches.erase(std::dynamic_pointer_cast<GroupMatch>(match)->get_group_qq());
      break;
    case DISCUSS_MATCH:
      discuss_matches.erase(std::dynamic_pointer_cast<DiscussMatch>(match)->get_discuss_qq());
      break;
    default:
      assert(match->get_type() == PRIVATE_MATCH);
  }

  /* Delete match from id. */
  matches.erase(id);

  match->broadcast("��Ϸ��ֹ�����");

  /* Delete players. */
  auto ready_iterators = match->get_ready_iterator();
  for (auto it = ready_iterators.first; it != ready_iterators.second; ++it)
  {
    DeletePlayer(*it);
  }
}


MatchId MatchManager::assign_id()
{
  /* If no available id, create new one. */
  if (match_ids.empty()) return next_match_id++;

  /* Or fetch from stack. */
  auto id = match_ids.top();
  match_ids.pop();
  return id;
}

bool MatchManager::PublicRequest(MessageIterator& msg, const std::unordered_map<QQ, std::shared_ptr<Match>> public_matches)
{
  auto player_match = player_matches.find(msg.usr_qq_);
  auto public_match = public_matches.find(msg.src_qq_);
  if (player_match == player_matches.end() ||
      public_match == public_matches.end() ||
      player_match->second->get_id() != public_match->second->get_id()) return false;
  player_match->second->Request(msg);
  return true;
}


Match::Match(const std::string& game_id, const int64_t& host_qq, const MatchType& type) :
  id_(id), type_(type), game_id_(game_id), host_qq_(host_qq), status_(PREPARE), game_(game_container.MakeGame(game_id_, *this))
{

}

bool Match::has_qq(const int64_t& qq) const
{
  return ready_qq_set_.find(qq) != ready_qq_set_.end();
}

void Match::Request(MessageIterator& msg)
{
  if (status_ != GAMING)
  {
    msg.Reply("���󣺵�ǰ��Ϸδ���ڽ���״̬");
    return;
  }
  assert(has_qq(msg.usr_qq_));
  game_->Request(qq2pid_[msg.usr_qq_], msg);
}

/* Register players, switch status and start the game 
*/
std::string Match::GameStart()
{
  if (status_ != PREPARE)
  {
    return "��ʼ��Ϸʧ�ܣ���Ϸδ����׼��״̬";
  }
  player_count_ = ready_qq_set_.size();
  if (player_count_ < game_->kMinPlayer)
  {
    return "��ʼ��Ϸʧ�ܣ������������";
  }
  for (auto qq : ready_qq_set_)
  {
    pid2qq_.push_back(qq);
    qq2pid_.emplace(qq, pid2qq_.size() - 1);
    game_->Join(game_container.MakePlayer(game_id_));
  }
  status_ = GAMING;
  game_->StartGame();
  return "";
}

/* append new player to qq_list
*/
std::string Match::Join(const int64_t& qq)
{
  assert(!has_qq(qq));
  if (status_ != PREPARE)
  {
    return "������Ϸʧ�ܣ���Ϸ�Ѿ���ʼ";
  }
  if (ready_qq_set_.size() >= game_->kMaxPlayer)
  {
    return "������Ϸʧ�ܣ����������Ѵﵽ��Ϸ����";
  }
  ready_qq_set_.insert(qq); // add to queue
  broadcast("��� " + LGTBOT::at_str(qq) + " ��������Ϸ");
  return "";
}

/* remove player from qq_list
*/
std::string Match::Leave(const int64_t& qq)
{
  assert(has_qq(qq));
  if (status_ != PREPARE)
  {
    return "�˳�ʧ�ܣ���Ϸ�Ѿ���ʼ�ˣ����Ӳ����ˣ�JOJO��";
  }
  broadcast("��� " + LGTBOT::at_str(qq) + " �˳�����Ϸ");
  ready_qq_set_.erase(qq);  // remove from queue
  return "";
}