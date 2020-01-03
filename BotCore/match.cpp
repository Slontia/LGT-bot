#include "stdafx.h"
#include "match.h"
#include "log.h"
#include "match.h"
#include "../new-rock-paper-scissors/dllmain.h"
#include "../new-rock-paper-scissors/game_base.h"
#include <optional>

MatchManager::MatchManager() : next_mid_(0) {}

std::string MatchManager::NewMatch(const GameHandle& game_handle, const UserID uid, const std::optional<GroupID> gid)
{
  if (uid2match_.find(uid) != uid2match_.end()) { return "���Ѽ�����Ϸ�������½���Ϸ"; }
  if (gid.has_value() && gid2match_.find(*gid) != gid2match_.end()) { return "�÷����Ѿ���ʼ����Ϸ���������½���Ϸ��"; }

  auto mid = NewMatchID();
  std::shared_ptr<Match> new_match = std::make_shared<Match>(mid, game_handle, uid, gid);
  assert(mid2match_.find(mid) == mid2match_.end());

  if (!mid2match_.emplace(mid, new_match).second) { assert(false); }
  if (!uid2match_.emplace(uid, new_match).second) { assert(false); }
  if (gid.has_value() && !gid2match_.emplace(*gid, new_match).second) { assert(false); }

  return "";
}

std::string MatchManager::StartGame(const UserID uid)
{
  auto it = uid2match_.find(uid);
  if (it == uid2match_.end()) { return "��δ������Ϸ"; }
  const std::shared_ptr<Match>& match = it->second;
  if (match->host_uid() != uid) { return "�����Ƿ�����û�п�ʼ��Ϸ��Ȩ��"; }
  RETURN_IF_FAILED(match->GameStart());
  return "";
}

std::string MatchManager::AddPlayer(const MatchId mid, const UserID uid)
{
  if (uid2match_.find(uid) != uid2match_.end()) { return "���Ѽ���������Ϸ�������˳�"; }
  const auto it = mid2match_.find(mid);
  if (it == mid2match_.end()) { return "��ϷID������"; }
  const std::shared_ptr<Match> match = it->second;
  RETURN_IF_FAILED(match->Join(uid));
  if (!uid2match_.emplace(uid, match).second) { assert(false); }
  return "";
}

std::string MatchManager::AddPlayer(const GroupID gid, const UserID uid)
{
  if (uid2match_.find(uid) != uid2match_.end()) { return "���Ѽ���������Ϸ�������˳�"; }
  const auto it = gid2match_.find(gid);
  if (it == gid2match_.end()) { return "�÷���δ������Ϸ"; }
  const std::shared_ptr<Match> match = it->second;
  RETURN_IF_FAILED(match->Join(uid));
  if (!uid2match_.emplace(uid, match).second) { assert(false); }
  return "";
}

std::string MatchManager::DeletePlayer(const UserID uid)
{
  auto it = uid2match_.find(uid);
  if (it == uid2match_.end()) { return "��δ������Ϸ���˳�ʧ��"; }
  const std::shared_ptr<Match>& match = it->second;
  RETURN_IF_FAILED(match->Leave(uid));
  uid2match_.erase(uid);
  /* If host quit, switch host. */
  if (uid == match->host_uid() && !match->SwitchHost()) { DeleteMatch(match->mid()); }
  return "";
}

/* return true if is a game request */
std::string MatchManager::Request(const UserID uid, const std::optional<GroupID> gid, const std::string& msg)
{
  const auto uid_it = uid2match_.find(uid);
  if (uid_it == uid2match_.end()) { return "[����] ��δ�����κ���Ϸ"; }
  else if (gid.has_value())
  {
    const auto gid_it = gid2match_.find(*gid);
    if (gid_it == gid2match_.end() || uid_it->second->mid() != gid_it->second->mid()) { return "[����] ��δ���ڵ�ǰȺ������"; }
    else { return uid_it->second->Request(uid, true /* is_public */, msg); }
  }
  else { return uid_it->second->Request(uid, false /* is_public */, msg); }
}

void MatchManager::DeleteMatch(const MatchId mid)
{
  auto it = mid2match_.find(mid);
  assert(it != mid2match_.end());
  const std::shared_ptr<Match> match = it->second;
  match->broadcast("��Ϸ��ֹ�����");

  mid2match_.erase(match->mid());
  if (match->gid().has_value()) { gid2match_.erase(*match->gid()); }
  const std::set<UserID>& ready_uid_set = match->ready_uid_set();
  for (auto it = ready_uid_set.begin(); it != ready_uid_set.end(); ++ it) { uid2match_.erase(*it); }
}

MatchId MatchManager::NewMatchID()
{
  while (mid2match_.find(++ next_mid_) != mid2match_.end());
  return next_mid_;
}

Match::Match(const MatchId mid, const GameHandle& game_handle, const UserID host_uid, const std::optional<GroupID> gid) :
  mid_(mid), game_handle_(game_handle), host_uid_(host_uid), gid_(gid), state_(PREPARE) {}

bool Match::Has(const UserID uid) const
{
  return ready_uid_set_.find(uid) != ready_uid_set_.end();
}

std::string Match::Request(const UserID uid, const std::optional<GroupID> gid, const std::string& msg)
{
  if (state_ != GAMING) { return "[����] ��ǰ��Ϸδ���ڽ���״̬"; }
  const auto it = uid2pid_.find(uid);
  assert(it != uid2pid_.end());
  return game_->HandleRequest(it->second, gid.has_value(), msg.c_str());
}

/* Register players, switch status and start the game 
*/
std::string Match::GameStart()
{
  if (state_ != PREPARE) { return "��ʼ��Ϸʧ�ܣ���Ϸδ����׼��״̬"; }
  const uint64_t player_num = ready_uid_set_.size();
  if (player_num < game_handle_.min_player_) { return "��ʼ��Ϸʧ�ܣ������������"; }
  assert(game_handle_.max_player_ == 0 || player_num <= game_handle_.max_player_);
  for (UserID uid : ready_uid_set_)
  {
    uid2pid_.emplace(uid, pid2uid_.size());
    pid2uid_.push_back(uid);
  }
  state_ = GAMING;
  game_ = NewGame(mid_, player_num);
  return "";
}

/* append new player to qq_list
*/
std::string Match::Join(const UserID uid)
{
  assert(!Has(uid));
  if (state_ != PREPARE) { return "������Ϸʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  if (ready_uid_set_.size() >= game_handle_.max_player_) { return "������Ϸʧ�ܣ����������Ѵﵽ��Ϸ����"; }
  ready_uid_set_.emplace(uid); // add to queue
  broadcast("��� " + At(uid) + " ��������Ϸ");
  return "";
}

/* remove player from qq_list
*/
std::string Match::Leave(const UserID uid)
{
  assert(Has(uid));
  if (state_ != PREPARE) { return "�˳�ʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  broadcast("��� " + At(uid) + " �˳�����Ϸ");
  ready_uid_set_.erase(uid);
  return "";
}