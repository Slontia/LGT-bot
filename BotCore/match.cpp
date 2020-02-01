#include "stdafx.h"
#include "match.h"
#include "log.h"
#include "match.h"
#include "../GameFramework/dllmain.h"
#include "../GameFramework/game_base.h"
#include <optional>

/* TODO:
 * 1. ��UserID GroupID MatchID�ýṹ���װ���ŵ���������Ŀ��util/Ŀ¼�£����GetMatch_ BindMatch_ UnbindMatch_����˽��ģ�巽��
 * 2. ���spinlock�࣬�Լ�spinlockguard�࣬�ŵ�util/Ŀ¼�£����й���������spinlock
 * 3. Game�����spinlock�����������thread�ص���Ҫ����
 * 4. ����GameOver�ӿڣ����thread�ص�������Ϸ��������is_over״̬��is_over״̬discard�������󣩵���GameOver�ӿڣ��ڲ�����DeleteMatch����
 */

std::map<MatchId, std::shared_ptr<Match>> MatchManager::mid2match_;
std::map<UserID, std::shared_ptr<Match>> MatchManager::uid2match_;
std::map<GroupID, std::shared_ptr<Match>> MatchManager::gid2match_;
MatchId MatchManager::next_mid_ = 0;
SpinLock MatchManager::spinlock_;

std::string MatchManager::NewMatch(const GameHandle& game_handle, const UserID uid, const std::optional<GroupID> gid)
{
  SpinLockGuard l(spinlock_);
  if (GetMatch_(uid, uid2match_)) { return "�½���Ϸʧ�ܣ����Ѽ�����Ϸ"; }
  if (gid.has_value() && GetMatch_(*gid, gid2match_)) { return "�½���Ϸʧ�ܣ��÷����Ѿ���ʼ��Ϸ"; }

  const MatchId mid = NewMatchID();
  std::shared_ptr<Match> new_match = std::make_shared<Match>(mid, game_handle, uid, gid);
  
  BindMatch_(mid, mid2match_, new_match);
  BindMatch_(uid, uid2match_, new_match);
  if (gid.has_value()) { BindMatch_(*gid, gid2match_, new_match); }

  return "";
}

std::string MatchManager::StartGame(const UserID uid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
  if (!match) { return "��ʼ��Ϸʧ�ܣ���δ������Ϸ"; }
  if (match->host_uid() != uid) { return "��ʼ��Ϸʧ�ܣ������Ƿ�����û�п�ʼ��Ϸ��Ȩ��"; }
  RETURN_IF_FAILED(match->GameStart());
  return "";
}

std::string MatchManager::AddPlayerWithMatchID(const MatchId mid, const UserID uid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
  if (!match) { return "������Ϸʧ�ܣ���ϷID������"; }
  return AddPlayer_(match, uid);
}

std::string MatchManager::AddPlayerWithGroupID(const GroupID gid, const UserID uid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match> match = GetMatch_(gid, gid2match_);
  if (!match) { return "������Ϸʧ�ܣ��÷���δ������Ϸ"; }
  return AddPlayer_(match, uid);
}

std::string MatchManager::AddPlayer_(const std::shared_ptr<Match>& match, const UserID uid)
{
  assert(match);
  if (GetMatch_(uid, uid2match_)) { return "������Ϸʧ�ܣ����Ѽ���������Ϸ�������˳�"; }
  RETURN_IF_FAILED(match->Join(uid));
  BindMatch_(uid, uid2match_, match);
  return "";
}

std::string MatchManager::DeletePlayer(const UserID uid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
  if (!match) { return "�˳���Ϸʧ�ܣ���δ������Ϸ"; }
  RETURN_IF_FAILED(match->Leave(uid));
  UnbindMatch_(uid, uid2match_);
  /* If host quit, switch host. */
  if (uid == match->host_uid() && !match->SwitchHost()) { DeleteMatch(match->mid()); }
  return "";
}

void MatchManager::DeleteMatch(const MatchId mid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
  assert(match);
  match->Boardcast("��Ϸ��ֹ�����");

  UnbindMatch_(mid, mid2match_);
  if (match->gid().has_value()) { UnbindMatch_(*match->gid(), gid2match_); }
  const std::set<UserID>& ready_uid_set = match->ready_uid_set();
  for (auto it = ready_uid_set.begin(); it != ready_uid_set.end(); ++ it) { UnbindMatch_(*it, uid2match_); }
}

std::shared_ptr<Match> MatchManager::GetMatch(const MatchId mid)
{
  SpinLockGuard l(spinlock_);
  return GetMatch_(mid, mid2match_);
}

std::shared_ptr<Match> MatchManager::GetMatch(const UserID uid, const std::optional<GroupID> gid)
{
  SpinLockGuard l(spinlock_);
  std::shared_ptr<Match> match = GetMatch_(uid, uid2match_);
  return (!match || (gid.has_value() && GetMatch_(*gid, gid2match_) != match)) ? nullptr : match;
}

MatchId MatchManager::NewMatchID()
{
  while (mid2match_.find(++ next_mid_) != mid2match_.end());
  return next_mid_;
}

Match::Match(const MatchId mid, const GameHandle& game_handle, const UserID host_uid, const std::optional<GroupID> gid) :
  mid_(mid), game_handle_(game_handle), host_uid_(host_uid), gid_(gid), state_(PREPARE) {}

Match::~Match()
{
  if (game_) { game_handle_.delete_game_(game_); }
}

bool Match::Has(const UserID uid) const
{
  return ready_uid_set_.find(uid) != ready_uid_set_.end();
}

std::string Match::Request(const UserID uid, const std::optional<GroupID> gid, const std::string& msg)
{
  if (state_ != GAMING) { return "[����] ��ǰ��Ϸδ���ڽ���״̬"; }
  const auto it = uid2pid_.find(uid);
  assert(it != uid2pid_.end());
  game_->HandleRequest(it->second, gid.has_value(), msg.c_str());
  return "";
}

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
  game_ = game_handle_.new_game_(mid_, player_num);
  return "";
}

std::string Match::Join(const UserID uid)
{
  assert(!Has(uid));
  if (state_ != PREPARE) { return "������Ϸʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  if (ready_uid_set_.size() >= game_handle_.max_player_) { return "������Ϸʧ�ܣ����������Ѵﵽ��Ϸ����"; }
  ready_uid_set_.emplace(uid);
  Boardcast("��� " + At(uid) + " ��������Ϸ");
  return "";
}

std::string Match::Leave(const UserID uid)
{
  assert(Has(uid));
  if (state_ != PREPARE) { return "�˳�ʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  Boardcast("��� " + At(uid) + " �˳�����Ϸ");
  ready_uid_set_.erase(uid);
  return "";
}

void Match::Boardcast(const std::string& msg) const
{
  if (gid_.has_value()) { SendPublicMsg(*gid_, msg); }
  else { for (const UserID uid : ready_uid_set_) { SendPrivateMsg(uid, msg); } }
}

void Match::Tell(const uint64_t pid, const std::string& msg) const
{
  SendPrivateMsg(pid2uid_[pid], msg);
}

void Match::At(const uint64_t pid, char* buf, const uint64_t len) const
{
  ::At(pid2uid_[pid], buf, len);
}

std::string Match::At(const uint64_t pid) const
{
  return ::At(pid2uid_[pid]);
}
