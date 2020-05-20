#include "stdafx.h"
#include "match.h"
#include "log.h"
#include "match.h"
#include "db_manager.h"
#include "../GameFramework/dllmain.h"
#include "../GameFramework/game_base.h"

std::map<MatchId, std::shared_ptr<Match>> MatchManager::mid2match_;
std::map<UserID, std::shared_ptr<Match>> MatchManager::uid2match_;
std::map<GroupID, std::shared_ptr<Match>> MatchManager::gid2match_;
MatchId MatchManager::next_mid_ = 0;
SpinLock MatchManager::spinlock_; // to lock match map

std::string MatchManager::NewMatch(const GameHandle& game_handle, const UserID uid, const std::optional<GroupID> gid, const bool skip_config)
{
  std::lock_guard<SpinLock> l(spinlock_);
  if (GetMatch_(uid, uid2match_)) { return "�½���Ϸʧ�ܣ����Ѽ�����Ϸ"; }
  if (gid.has_value() && GetMatch_(*gid, gid2match_)) { return "�½���Ϸʧ�ܣ��÷����Ѿ���ʼ��Ϸ"; }

  const MatchId mid = NewMatchID_();
  std::shared_ptr<Match> new_match = std::make_shared<Match>(mid, game_handle, uid, gid, skip_config);
  
  RETURN_IF_FAILED(AddPlayer_(new_match, uid));
  BindMatch_(mid, mid2match_, new_match);
  if (gid.has_value()) { BindMatch_(*gid, gid2match_, new_match); }

  return "";
}

std::string MatchManager::ConfigOver(const UserID uid, const std::optional<GroupID> gid)
{
  std::lock_guard<SpinLock> l(spinlock_);
  const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
  if (!match) { return "��������ʧ�ܣ���δ������Ϸ"; }
  else if (match->host_uid() != uid) { return "��������ʧ�ܣ������Ƿ�����û�н������õ�Ȩ��"; }
  else if (!match->IsPrivate() && !gid.has_value()) { return "��������ʧ�ܣ��빫����������"; }
  else if (match->gid() != gid) { return "��������ʧ�ܣ���δ�ڸ÷��佨����Ϸ"; }
  RETURN_IF_FAILED(match->GameConfigOver());
  return "";
}

std::string MatchManager::StartGame(const UserID uid, const std::optional<GroupID> gid)
{
  std::lock_guard<SpinLock> l(spinlock_);
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
  std::lock_guard<SpinLock> l(spinlock_);
  const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
  if (!match) { return "������Ϸʧ�ܣ���ϷID������"; }
  else if (!match->IsPrivate()) { return "������Ϸʧ�ܣ�����Ϸ���ڹ�����������ǰ�����������Ϸ"; }
  return AddPlayer_(match, uid);
}

std::string MatchManager::AddPlayerToPublicGame(const GroupID gid, const UserID uid)
{
  std::lock_guard<SpinLock> l(spinlock_);
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
  std::lock_guard<SpinLock> l(spinlock_);
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
  std::lock_guard<SpinLock> l(spinlock_);
  return DeleteMatch_(mid);
}

void MatchManager::DeleteMatch_(const MatchId mid)
{
  if (const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_); match)
  {
		UnbindMatch_(mid, mid2match_);
		if (match->gid().has_value()) { UnbindMatch_(*match->gid(), gid2match_); }
		const std::set<UserID>& ready_uid_set = match->ready_uid_set();
		for (auto it = ready_uid_set.begin(); it != ready_uid_set.end(); ++ it) { UnbindMatch_(*it, uid2match_); }
  }
}

std::shared_ptr<Match> MatchManager::GetMatch(const MatchId mid)
{
  std::lock_guard<SpinLock> l(spinlock_);
  return GetMatch_(mid, mid2match_);
}

std::shared_ptr<Match> MatchManager::GetMatch(const UserID uid, const std::optional<GroupID> gid)
{
  std::lock_guard<SpinLock> l(spinlock_);
  std::shared_ptr<Match> match = GetMatch_(uid, uid2match_);
  return (!match || (gid.has_value() && GetMatch_(*gid, gid2match_) != match)) ? nullptr : match;
}

std::shared_ptr<Match> MatchManager::GetMatchWithGroupID(const GroupID gid)
{
  std::lock_guard<SpinLock> l(spinlock_);
  return GetMatch_(gid, gid2match_);
}

void MatchManager::ForEachMatch(const std::function<void(const std::shared_ptr<Match>)> handle)
{
  std::lock_guard<SpinLock> l(spinlock_);
  for (const auto&[_, match] : mid2match_) { handle(match); }
}

MatchId MatchManager::NewMatchID_()
{
  while (mid2match_.find(++ next_mid_) != mid2match_.end());
  return next_mid_;
}

Match::Match(const MatchId mid, const GameHandle& game_handle, const UserID host_uid, const std::optional<GroupID> gid, const bool skip_config)
  : mid_(mid), game_handle_(game_handle), host_uid_(host_uid), gid_(gid), state_(skip_config ? State::NOT_STARTED : State::IN_CONFIGURING),
  game_(game_handle_.new_game_(this), game_handle_.delete_game_), multiple_(1) {}

Match::~Match() {}

bool Match::Has(const UserID uid) const
{
  return ready_uid_set_.find(uid) != ready_uid_set_.end();
}

std::string Match::Request(const UserID uid, const std::optional<GroupID> gid, const std::string& msg)
{
  if (state_ == State::NOT_STARTED) { return "[����] ��ǰ�׶εȴ���Ҽ��룬�޷�ִ����Ϸ����"; }
  if (state_ == State::IN_CONFIGURING) { game_->HandleRequest(0, gid.has_value(), msg.c_str()); }
  else
  {
		const auto it = uid2pid_.find(uid);
		assert(it != uid2pid_.end());
		game_->HandleRequest(it->second, gid.has_value(), msg.c_str());
  }
  return "";
}

std::string Match::GameConfigOver()
{
  if (state_ != State::IN_CONFIGURING) { return "��������ʧ�ܣ���Ϸδ��������״̬"; }
  state_ = State::NOT_STARTED;
  return "���ý�����������ҿ��Լ��������";
}

std::string Match::GameStart()
{
  if (state_ == State::IN_CONFIGURING) { return "��ʼ��Ϸʧ�ܣ���Ϸ���������ý׶Σ���������ã��ȴ���Ҽ�����ٿ�ʼ��Ϸ"; }
  if (state_ == State::IS_STARTED) { return "��ʼ��Ϸʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  const uint64_t player_num = ready_uid_set_.size();
  if (player_num < game_handle_.min_player_) { return "��ʼ��Ϸʧ�ܣ������������"; }
  assert(game_handle_.max_player_ == 0 || player_num <= game_handle_.max_player_);
  if (!game_->StartGame(player_num)) { return "��ʼ��Ϸʧ�ܣ���������Ϸ������Ԥ��"; }

  for (UserID uid : ready_uid_set_)
  {
    uid2pid_.emplace(uid, pid2uid_.size());
    pid2uid_.push_back(uid);
  }
  state_ = State::IS_STARTED;
  BoardcastPlayers("��Ϸ��ʼ��������ʹ��<����>�����#�ţ����鿴��ִ������");
  start_time_ = std::chrono::system_clock::now();
  return "";
}

std::string Match::Join(const UserID uid)
{
  assert(!Has(uid));
  if (state_ == State::IN_CONFIGURING && uid != host_uid_) { return "������Ϸʧ�ܣ���������������Ϸ����"; }
  if (state_ == State::IS_STARTED) { return "������Ϸʧ�ܣ���Ϸ�Ѿ���ʼ"; }
  if (ready_uid_set_.size() >= game_handle_.max_player_) { return "������Ϸʧ�ܣ����������Ѵﵽ��Ϸ����"; }
  ready_uid_set_.emplace(uid);
  BoardcastPlayers("��� " + At(uid) + " ��������Ϸ");
  return "";
}

std::string Match::Leave(const UserID uid)
{
  assert(Has(uid));
  assert(state_ != State::IN_CONFIGURING);
  if (state_ == State::IS_STARTED) { return "�˳�ʧ�ܣ���Ϸ�Ѿ���ʼ"; }
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
  ::At(state_ != State::IS_STARTED ? host_uid_ : pid2uid_[pid]);
}

std::string Match::AtPlayer(const uint64_t pid) const
{
  return ::At(pid2uid_[pid]);
}

void Match::GameOver(const int64_t scores[])
{
  if (!scores)
  {
    BoardcastPlayers("��Ϸ�жϣ��þ���Ϸ�ɼ���Ч����л��λ���룡");
    return;
  }
  end_time_ = std::chrono::system_clock::now();
  assert(ready_uid_set_.size() == pid2uid_.size());
  std::vector<Match::ScoreInfo> score_info = CalScores_(scores);
  std::ostringstream ss;
  ss << "��Ϸ����������������" << std::endl;
  for (uint64_t pid = 0; pid < pid2uid_.size(); ++pid) { ss << AtPlayer(pid) << " " << scores[pid] << std::endl; }
  ss << "��л��λ���룡";
  BoardcastPlayers(ss.str());
  if (auto& db_manager = DBManager::GetDBManager(); db_manager != nullptr)
  {
    std::optional<uint64_t> game_id = game_handle_.game_id_.load();
    if (game_id.has_value() && !db_manager->RecordMatch(*game_id, gid_, host_uid_, multiple_, score_info))
    {
      ss << std::endl << "������Ϸ���д�����ݿ�ʧ�ܣ�����ϵ����Ա";
    }
  }
}

std::vector<Match::ScoreInfo> Match::CalScores_(const int64_t scores[]) const
{
  const uint64_t player_num = pid2uid_.size();
  std::vector<Match::ScoreInfo> ret(player_num);
  const double avg_score = [scores, player_num]
  {
    double score_sum = 0;
    for (uint64_t pid = 0; pid < player_num; ++pid) { score_sum += scores[pid]; }
    return score_sum / player_num;
  }();
  const double delta = [avg_score, scores, player_num]
  {
    double score_offset_sum = 0;
    for (uint64_t pid = 0; pid < player_num; ++pid)
    {
      double offset = scores[pid] - avg_score;
      score_offset_sum += offset > 0 ? offset : -offset;
    }
    return 1.0 / score_offset_sum;
  }();
  for (uint64_t pid = 0; pid < player_num; ++pid)
  {
    Match::ScoreInfo& score_info = ret[pid];
    score_info.uid_ = pid2uid_[pid];
    score_info.game_score_ = scores[pid];
    score_info.zero_sum_match_score_ = delta * (scores[pid] - avg_score);
    score_info.poss_match_score_ = 0;
  }
  return ret;
}

bool Match::SwitchHost()
{
  if (ready_uid_set_.empty()) { return false; }
  host_uid_ = *ready_uid_set_.begin();
  BoardcastPlayers(At(host_uid_) + "��ѡΪ�·���");
  return true;
}

void Match::StartTimer(const uint64_t sec)
{
  static const uint64_t kMinAlertSec = 10;
  if (sec == 0) { return; }
  Timer::TaskSet tasks;
  std::shared_ptr<bool> stage_is_over = std::make_shared<bool>(false);
  std::function<void(Timer*)> deleter = [stage_is_over](Timer* timer)
  {
    *stage_is_over = true;
    delete timer;
  };
  std::function<void()> timeout_handler = [match = shared_from_this(), stage_is_over]()
  {
    match->game_->HandleTimeout(stage_is_over.get());
  };
  if (kMinAlertSec > sec / 2)
  {
    tasks.emplace_front(sec, timeout_handler);
  }
  else
  {
    tasks.emplace_front(kMinAlertSec, timeout_handler);
		for (uint64_t alert_sec = kMinAlertSec, sum_alert_sec = kMinAlertSec; sum_alert_sec < sec / 2; sum_alert_sec += alert_sec, alert_sec *= 2)
		{
			tasks.emplace_front(alert_sec, [this, alert_sec] { BoardcastPlayers("ʣ��ʱ��" + std::to_string(alert_sec / 60) + "��" + std::to_string(alert_sec % 60) + "��"); });
		}
  }
  timer_ = std::unique_ptr<Timer, std::function<void(Timer*)>>(new Timer(std::move(tasks)), std::move(deleter));
}

void Match::StopTimer()
{
  timer_ = nullptr;
}

std::string Match::OptionInfo() const
{
  return game_->OptionInfo();
}
