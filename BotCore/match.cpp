#include "stdafx.h"
#include "match.h"
#include "log.h"
#include "match.h"
#include "../GameFramework/dllmain.h"
#include "../GameFramework/game_base.h"
#include <optional>

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
  
  RETURN_IF_FAILED(AddPlayer_(new_match, uid));
  BindMatch_(mid, mid2match_, new_match);
  if (gid.has_value()) { BindMatch_(*gid, gid2match_, new_match); }

  return "";
}

std::string MatchManager::StartGame(const UserID uid, const std::optional<GroupID> gid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
  if (!match) { return "��ʼ��Ϸʧ�ܣ���δ������Ϸ"; }
  else if (match->host_uid() != uid) { return "��ʼ��Ϸʧ�ܣ������Ƿ�����û�п�ʼ��Ϸ��Ȩ��"; }
  else if (!match->IsPrivate() && !gid.has_value()) { return "��ʼ��Ϸʧ�ܣ��빫����ʼ��Ϸ"; }
  else if (match->gid() != gid) { return "��ʼ��Ϸʧ�ܣ���δ�ڸ÷��佨����Ϸ"; }
  RETURN_IF_FAILED(match->GameStart());
  return "";
}

std::string MatchManager::AddPlayerToPrivateGame(const MatchId mid, const UserID uid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
  if (!match) { return "������Ϸʧ�ܣ���ϷID������"; }
  else if (!match->IsPrivate()) { return "������Ϸʧ�ܣ�����Ϸ���ڹ�����������ǰ�����������Ϸ"; }
  return AddPlayer_(match, uid);
}

std::string MatchManager::AddPlayerToPublicGame(const GroupID gid, const UserID uid)
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

std::string MatchManager::DeletePlayer(const UserID uid, const std::optional<GroupID> gid)
{
  SpinLockGuard l(spinlock_);
  const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
  if (!match) { return "�˳���Ϸʧ�ܣ���δ������Ϸ"; }
  else if (!match->IsPrivate() && !gid.has_value()) { return "�˳���Ϸʧ�ܣ��빫���˳���Ϸ"; }
  else if (match->gid() != gid) { return "�˳���Ϸʧ�ܣ���δ���뱾������Ϸ"; }
  RETURN_IF_FAILED(match->Leave(uid));
  UnbindMatch_(uid, uid2match_);
  /* If host quit, switch host. */
  if (uid == match->host_uid() && !match->SwitchHost()) { DeleteMatch_(match->mid()); }
  return "";
}

void MatchManager::DeleteMatch(const MatchId mid)
{
  SpinLockGuard l(spinlock_);
  return DeleteMatch_(mid);
}

void MatchManager::DeleteMatch_(const MatchId mid)
{
  const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
  assert(match);
  match->BoardcastPlayers("��Ϸ����");

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

Match::Match(const MatchId mid, const GameHandle& game_handle, const UserID host_uid, const std::optional<GroupID> gid)
  : mid_(mid), game_handle_(game_handle), host_uid_(host_uid), gid_(gid), state_(PREPARE), game_(nullptr) {}

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
  game_ = game_handle_.new_game_(this, player_num);
  return "";
}

std::string Match::Join(const UserID uid)
{
  assert(!Has(uid));
  if (state_ != PREPARE) { return "������Ϸʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  if (ready_uid_set_.size() >= game_handle_.max_player_) { return "������Ϸʧ�ܣ����������Ѵﵽ��Ϸ����"; }
  ready_uid_set_.emplace(uid);
  BoardcastPlayers("��� " + At(uid) + " ��������Ϸ");
  return "";
}

std::string Match::Leave(const UserID uid)
{
  assert(Has(uid));
  if (state_ != PREPARE) { return "�˳�ʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  BoardcastPlayers("��� " + At(uid) + " �˳�����Ϸ");
  ready_uid_set_.erase(uid);
  return "";
}

void Match::BoardcastPlayers(const std::string& msg) const
{
  if (gid_.has_value()) { SendPublicMsg(*gid_, msg); }
  else { for (const UserID uid : ready_uid_set_) { SendPrivateMsg(uid, msg); } }
}

void Match::TellPlayer(const uint64_t pid, const std::string& msg) const
{
  SendPrivateMsg(pid2uid_[pid], msg);
}

void Match::AtPlayer(const uint64_t pid, char* buf, const uint64_t len) const
{
  ::At(pid2uid_[pid], buf, len);
}

std::string Match::AtPlayer(const uint64_t pid) const
{
  return ::At(pid2uid_[pid]);
}

void Match::GameOver(const int64_t scores[])
{
  assert(ready_uid_set_.size() == pid2uid_.size());
  std::ostringstream ss;
  ss << "��Ϸ����������������" << std::endl;
  for (uint64_t pid = 0; pid < pid2uid_.size(); ++pid) { ss << AtPlayer(pid) << " " << scores[pid] << std::endl; }
  ss << "��л��Ҳ��룡";
  BoardcastPlayers(ss.str());
  //TODO: link to database
}

bool Match::SwitchHost()
{
  if (ready_uid_set_.empty()) { return false; }
  host_uid_ = *ready_uid_set_.begin();
  BoardcastPlayers(At(host_uid_) + "��ѡΪ�·���");
  return true;
}
