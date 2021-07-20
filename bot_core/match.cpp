#include "match.h"

#include <assert.h>

#include "db_manager.h"
#include "game_framework/game_base.h"
#include "game_framework/game_main.h"
#include "log.h"
#include "match.h"
#include "util.h"

// func can appear only once
#define RETURN_IF_FAILED(func)                                 \
    do {                                                       \
        if (const auto ret = (func); ret != EC_OK) return ret; \
    } while (0);

static void BoardcastMatchCanJoin(const Match& match)
{
    if (match.gid().has_value()) {
        match.Boardcast() << "现在玩家可以在群里通过\"#加入游戏\"报名比赛";
    } else {
        match.Boardcast() << "现在玩家可以通过私信我\"#加入游戏 " << match.mid() << "\"报名比赛";
    }
}

ErrCode MatchManager::NewMatch(const GameHandle& game_handle, const UserID uid, const std::optional<GroupID> gid,
                               const bool skip_config, const replier_t reply)
{
    std::lock_guard<SpinLock> l(spinlock_);
    if (GetMatch_(uid, uid2match_)) {
        reply() << "[错误] 建立失败：您已加入游戏";
        return EC_MATCH_USER_ALREADY_IN_MATCH;
    }
    if (gid.has_value() && GetMatch_(*gid, gid2match_)) {
        reply() << "[错误] 建立失败：该房间已经开始游戏";
        return EC_MATCH_ALREADY_BEGIN;
    }
    const MatchID mid = NewMatchID_();
    std::shared_ptr<Match> new_match = std::make_shared<Match>(bot_, mid, game_handle, uid, gid, skip_config);
    RETURN_IF_FAILED(AddPlayer_(new_match, uid, reply));
    BindMatch_(mid, mid2match_, new_match);
    if (gid.has_value()) {
        BindMatch_(*gid, gid2match_, new_match);
    }
    if (skip_config) {
        BoardcastMatchCanJoin(*new_match);
    }
    return EC_OK;
}

std::variant<ErrCode, std::shared_ptr<Match>> MatchManager::UnsafeGetMatchByHost_(
        const UserID uid, const std::optional<GroupID> gid, const replier_t reply)
{
    const std::shared_ptr<Match>& match = GetMatch_(uid, uid2match_);
    if (!match) {
        reply() << "[错误] 开始失败：您未加入游戏";
        return EC_MATCH_USER_NOT_IN_MATCH;
    }
    if (match->host_uid() != uid) {
        reply() << "[错误] 开始失败：您不是房主，没有开始游戏的权限";
        return EC_MATCH_NOT_HOST;
    }
    if (gid.has_value() && match->gid() != gid) {
        reply() << "[错误] 开始失败：您未在该房间建立游戏";
        return EC_MATCH_NOT_THIS_GROUP;
    }
    return match;
}

ErrCode MatchManager::ConfigOver(const UserID uid, const std::optional<GroupID> gid, const replier_t reply)
{
    std::lock_guard<SpinLock> l(spinlock_);
    const auto match_or_errcode = UnsafeGetMatchByHost_(uid, gid, reply);
    if (const auto p_errcode = std::get_if<0>(&match_or_errcode)) {
        return *p_errcode;
    }
    const auto match = std::get<1>(match_or_errcode);
    return match->GameConfigOver(reply);
}

ErrCode MatchManager::SetComNum(const UserID uid, const std::optional<GroupID> gid, const replier_t reply, const uint64_t com_num)
{
    std::lock_guard<SpinLock> l(spinlock_);
    const auto match_or_errcode = UnsafeGetMatchByHost_(uid, gid, reply);
    if (const auto p_errcode = std::get_if<0>(&match_or_errcode)) {
        return *p_errcode;
    }
    const auto match = std::get<1>(match_or_errcode);
    return match->GameSetComNum(reply, com_num);
}

ErrCode MatchManager::StartGame(const UserID uid, const std::optional<GroupID> gid, const replier_t reply)
{
    std::lock_guard<SpinLock> l(spinlock_);
    const auto match_or_errcode = UnsafeGetMatchByHost_(uid, gid, reply);
    if (const auto p_errcode = std::get_if<0>(&match_or_errcode)) {
        return *p_errcode;
    }
    const auto match = std::get<1>(match_or_errcode);
    return match->GameStart(gid.has_value(), reply);
}

ErrCode MatchManager::AddPlayerToPrivateGame(const MatchID mid, const UserID uid, const replier_t reply)
{
    std::lock_guard<SpinLock> l(spinlock_);
    const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_);
    if (!match) {
        reply() << "[错误] 加入失败：游戏ID不存在";
        return EC_MATCH_NOT_EXIST;
    }
    if (!match->IsPrivate()) {
        reply() << "[错误] 加入失败：该游戏属于公开比赛，请前往房间加入游戏";
        return EC_MATCH_NEED_REQUEST_PUBLIC;
    }
    return AddPlayer_(match, uid, reply);
}

ErrCode MatchManager::AddPlayerToPublicGame(const GroupID gid, const UserID uid, const replier_t reply)
{
    std::lock_guard<SpinLock> l(spinlock_);
    const std::shared_ptr<Match> match = GetMatch_(gid, gid2match_);
    if (!match) {
        reply() << "[错误] 加入失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    }
    return AddPlayer_(match, uid, reply);
}

ErrCode MatchManager::AddPlayer_(const std::shared_ptr<Match>& match, const UserID uid, const replier_t reply)
{
    assert(match);
    if (const std::shared_ptr<Match>& user_match = GetMatch_(uid, uid2match_); user_match == match) {
        reply() << "[错误] 加入失败：您已加入该游戏";
        return EC_MATCH_USER_ALREADY_IN_MATCH;
    } else if (user_match != nullptr) {
        reply() << "[错误] 加入失败：您已加入其它游戏，请先退出";
        return EC_MATCH_USER_ALREADY_IN_OTHER_MATCH;
    }
    RETURN_IF_FAILED(match->Join(uid, reply));
    BindMatch_(uid, uid2match_, match);
    return EC_OK;
}

ErrCode MatchManager::DeletePlayer(const UserID uid, const std::optional<GroupID> gid, const replier_t reply, const bool force)
{
    std::shared_ptr<Match> match = nullptr;
    {
        std::lock_guard<SpinLock> l(spinlock_);
        match = GetMatch_(uid, uid2match_);
        if (!match) {
            reply() << "[错误] 退出失败：您未加入游戏";
            return EC_MATCH_USER_NOT_IN_MATCH;
        }
        if (match->gid() != gid) {
            reply() << "[错误] 退出失败：您未加入本房间游戏";
            return EC_MATCH_NOT_THIS_GROUP;
        }

        RETURN_IF_FAILED(match->Leave(uid, reply, force));

        // It is save to unbind player before game over because erase do nothing when element not in map.
        UnbindMatch_(uid, uid2match_);

        // If host quit, switch host.
        if (uid == match->host_uid() && !match->SwitchHost()) {
            DeleteMatch_(match->mid());
        }
    }
    if (force) {
        // Call LeaveMidway without match lock to prevent dead lock when game over and DeleteMatch.
        return match->LeaveMidway(uid, gid.has_value());
    }
    return EC_OK;
}

ErrCode MatchManager::DeleteMatch(const MatchID mid)
{
    std::lock_guard<SpinLock> l(spinlock_);
    DeleteMatch_(mid);
    return EC_OK;
}

void MatchManager::DeleteMatch_(const MatchID mid)
{
    if (const std::shared_ptr<Match> match = GetMatch_(mid, mid2match_); match) {
        UnbindMatch_(mid, mid2match_);
        if (match->gid().has_value()) {
            UnbindMatch_(*match->gid(), gid2match_);
        }
        const std::set<UserID>& ready_uid_set = match->ready_uid_set();
        for (auto it = ready_uid_set.begin(); it != ready_uid_set.end(); ++it) {
            UnbindMatch_(*it, uid2match_);
        }
    }
}

std::shared_ptr<Match> MatchManager::GetMatch(const MatchID mid)
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

std::shared_ptr<Match> MatchManager::GetMatch(const GroupID gid)
{
    std::lock_guard<SpinLock> l(spinlock_);
    return GetMatch_(gid, gid2match_);
}

void MatchManager::ForEachMatch(const std::function<void(const std::shared_ptr<Match>)> handle)
{
    std::lock_guard<SpinLock> l(spinlock_);
    for (const auto& [_, match] : mid2match_) {
        handle(match);
    }
}

MatchID MatchManager::NewMatchID_()
{
    while (mid2match_.find(++next_mid_) != mid2match_.end())
        ;
    return next_mid_;
}

template <typename IDType>
std::shared_ptr<Match> MatchManager::GetMatch_(const IDType id,
                                               const std::map<IDType, std::shared_ptr<Match>>& id2match)
{
    const auto it = id2match.find(id);
    return (it == id2match.end()) ? nullptr : it->second;
}

template <typename IDType>
void MatchManager::BindMatch_(const IDType id, std::map<IDType, std::shared_ptr<Match>>& id2match,
                              std::shared_ptr<Match> match)
{
    if (!id2match.emplace(id, match).second) {
        assert(false);
    }
}

template <typename IDType>
void MatchManager::UnbindMatch_(const IDType id, std::map<IDType, std::shared_ptr<Match>>& id2match)
{
    id2match.erase(id);
}

Match::Match(BotCtx& bot, const MatchID mid, const GameHandle& game_handle, const UserID host_uid,
             const std::optional<GroupID> gid, const bool skip_config)
        : bot_(bot),
          mid_(mid),
          game_handle_(game_handle),
          host_uid_(host_uid),
          gid_(gid),
          state_(skip_config ? State::NOT_STARTED : State::IN_CONFIGURING),
          game_(game_handle_.new_game_(this), game_handle_.delete_game_),
          player_num_each_user_(1),
          com_num_(0),
          multiple_(1)
{
}

Match::~Match() {}

bool Match::Has(const UserID uid) const { return ready_uid_set_.find(uid) != ready_uid_set_.end(); }

Match::VariantID Match::ConvertPid(const PlayerID pid) const
{
    if (!pid.IsValid()) {
        return host_uid_; // TODO: UINT64_MAX for host
    }
    return players_[pid];
}

ErrCode Match::GameSetComNum(const replier_t reply, const uint64_t com_num)
{
    if (state_ == State::IS_STARTED) {
        reply() << "[错误] 游戏已经开始，无法设置AI数量";
        return EC_MATCH_ALREADY_BEGIN;
    }
    const auto old_com_num = std::exchange(com_num_, com_num);
    if (!SatisfyMaxPlayer_()) {
        reply() << "[错误] 设置失败：比赛人数将超过上限" << game_handle_.max_player_ << "人";
        com_num_ = old_com_num;
        return EC_MATCH_ACHIEVE_MAX_PLAYER;
    }
    return EC_OK;
}

ErrCode Match::Request(const UserID uid, const std::optional<GroupID> gid, const std::string& msg,
                       const replier_t reply)
{
    if (state_ == State::NOT_STARTED) {
        reply() << "[错误] 当前阶段等待玩家加入，无法执行游戏请求\n"
                   "若您想执行元指令，请尝试在请求前加\"#\"，或通过\"#帮助\"查看所有支持的元指令";
        return EC_MATCH_NOT_BEGIN;
    }
    if (state_ == State::IN_CONFIGURING) {
        return game_->HandleRequest(0, gid.has_value(), msg.c_str());
    } else {
        const auto it = uid2pid_.find(uid);
        assert(it != uid2pid_.end());
        return game_->HandleRequest(it->second, gid.has_value(), msg.c_str());
    }
}

ErrCode Match::GameConfigOver(const replier_t reply)
{
    if (state_ != State::IN_CONFIGURING) {
        reply() << "[错误] 结束配置失败：游戏未处于配置状态";
        return EC_MATCH_NOT_IN_CONFIG;
    }
    state_ = State::NOT_STARTED;
    BoardcastMatchCanJoin(*this);
    return EC_OK;
}

ErrCode Match::GameStart(const bool is_public, const replier_t reply)
{
    if (state_ == State::IN_CONFIGURING) {
        reply() << "[错误] 开始失败：游戏正处于配置阶段，请结束配置，等待玩家加入后再开始游戏";
        return EC_MATCH_IN_CONFIG;
    }
    if (state_ == State::IS_STARTED) {
        reply() << "[错误] 开始失败：游戏已经开始";
        return EC_MATCH_ALREADY_BEGIN;
    }
    const uint64_t player_num = ready_uid_set_.size();
    if (!game_->StartGame(is_public, player_num, 0)) {
        reply() << "[错误] 开始失败：不符合游戏参数的预期";
        return EC_MATCH_UNEXPECTED_CONFIG;
    }

    return EC_OK;
}

ErrCode Match::Join(const UserID uid, const replier_t reply)
{
    assert(!Has(uid));
    if (state_ == State::IN_CONFIGURING && uid != host_uid_) {
        reply() << "[错误] 加入失败：房主正在配置游戏参数";
        return EC_MATCH_IN_CONFIG;
    }
    if (state_ == State::IS_STARTED) {
        reply() << "[错误] 加入失败：游戏已经开始";
        return EC_MATCH_ALREADY_BEGIN;
    }
    if (game_handle_.max_player_ != 0 && ready_uid_set_.size() >= game_handle_.max_player_) {
        reply() << "[错误] 加入失败：比赛人数已达到游戏上限";
        return EC_MATCH_ACHIEVE_MAX_PLAYER;
    }
    ready_uid_set_.emplace(uid);
    Boardcast() << "玩家 " << AtMsg(uid) << " 加入了游戏";
    return EC_OK;
}

ErrCode Match::Leave(const UserID uid, const replier_t reply, const bool force)
{
    assert(Has(uid));
    if (state_ != State::IS_STARTED) {
        Boardcast() << "玩家 " << AtMsg(uid) << " 退出了游戏";
        ready_uid_set_.erase(uid);
        return EC_OK;
    }
    if (force) {
        Boardcast() << "玩家 " << AtMsg(uid) << " 中途退出了游戏，他将不再参与后续的游戏进程";
        ready_uid_set_.erase(uid);
        return EC_OK;
    }
    reply() << "[错误] 退出失败：游戏已经开始，若仍要退出游戏，请使用<#强制退出游戏>命令";
    return EC_MATCH_ALREADY_BEGIN;
}

ErrCode Match::LeaveMidway(const UserID uid, const bool is_public)
{
    if (state_ == State::IS_STARTED) {
        const auto it = uid2pid_.find(uid);
        assert(it != uid2pid_.end());
        return game_->HandleLeave(it->second, is_public);
    }
    return EC_OK;
}

MsgSenderWrapper<MsgSenderForBot> Match::Boardcast() const
{
    if (gid_.has_value()) {
        return MsgSenderWrapper(bot_.ToGroupRaw(*gid_));
    } else {
        MsgSenderWrapper<MsgSenderForBot>::Container senders;
        for (const UserID uid : ready_uid_set_) {
            senders.emplace_back(bot_.ToUserRaw(uid));
        }
        return MsgSenderWrapper(std::move(senders));
    }
}

MsgSenderWrapper<MsgSenderForBot> Match::Tell(const uint64_t pid) const
{
    const auto& id = ConvertPid(pid);
    if (const auto pval = std::get_if<UserID>(&id)) {
        return bot_.ToUser(*pval);
    } else {
        return {};
    }
}

void Match::GamePrepare()
{
    // TODO: check player_num_each_user_
    state_ = State::IS_STARTED;
    for (UserID uid : ready_uid_set_) {
        uid2pid_.emplace(uid, players_.size());
        players_.emplace_back(uid);
    }
    for (ComputerID cid = 0; cid < com_num_; ++cid) {
        players_.emplace_back(cid);
    }
    Boardcast() << "游戏开始，您可以使用<帮助>命令（无#号），查看可执行命令";
    start_time_ = std::chrono::system_clock::now();
}

void Match::GameOver(const int64_t scores[])
{
    if (!scores) {
        Boardcast() << "游戏中断，该局游戏成绩无效，感谢诸位参与！";
        return;
    }
    end_time_ = std::chrono::system_clock::now();
    auto sender = Boardcast();
    sender << "游戏结束，公布分数：\n";
    for (PlayerID pid = 0; pid < PlayerNum(); ++pid) {
        PrintPlayer<true>(sender, pid);
        sender << " " << scores[pid] << "\n";
    }
    sender << "感谢诸位参与！";
#ifdef WITH_MYSQL
    const std::vector<Match::ScoreInfo> score_info = CalScores_(scores);
    if (auto& db_manager = DBManager::GetDBManager(); !db_manager) {
        sender << "\n[警告] 未连接数据库，游戏结果不会被记录";
    } else if (std::optional<uint64_t> game_id = game_handle_.game_id_.load(); !game_id.has_value()) {
        sender << "\n[警告] 该游戏未发布，游戏结果不会被记录";
    } else if (!db_manager->RecordMatch(*game_id, gid_, host_uid_, multiple_, score_info)) {
        sender << "\n[错误] 游戏结果写入数据库失败，请联系管理员";
    } else {
        sender << "\n游戏结果写入数据库成功！";
    }
#endif
}

std::vector<Match::ScoreInfo> Match::CalScores_(const int64_t scores[]) const
{
    const uint64_t user_num = uid2pid_.size();
    std::vector<Match::ScoreInfo> ret(user_num);
    const double avg_score = [scores, user_num] {
        double score_sum = 0;
        for (uint64_t pid = 0; pid < user_num; ++pid) {
            score_sum += scores[pid];
        }
        return score_sum / user_num;
    }();
    const double delta = [avg_score, scores, user_num] {
        double score_offset_sum = 0;
        for (uint64_t pid = 0; pid < user_num; ++pid) {
            double offset = scores[pid] - avg_score;
            score_offset_sum += offset > 0 ? offset : -offset;
        }
        return 1.0 / score_offset_sum;
    }();
    for (PlayerID pid = 0; pid < user_num; ++pid) {
        if (const auto pval = std::get_if<UserID>(&players_[pid])) {
            Match::ScoreInfo& score_info = ret[pid];
            score_info.uid_ = *pval;
            score_info.game_score_ = scores[pid];
            score_info.zero_sum_match_score_ = delta * (scores[pid] - avg_score);
            score_info.poss_match_score_ = 0;
        }
    }
    return ret;
}

bool Match::SwitchHost()
{
    if (ready_uid_set_.empty()) {
        return false;
    }
    if (state_ == NOT_STARTED) {
        host_uid_ = *ready_uid_set_.begin();
        Boardcast() << AtMsg(host_uid_) << "被选为新房主";
    }
    return true;
}

void Match::StartTimer(const uint64_t sec)
{
    static const uint64_t kMinAlertSec = 10;
    if (sec == 0) {
        return;
    }
    Timer::TaskSet tasks;
    std::shared_ptr<bool> stage_is_over = std::make_shared<bool>(false);
    std::function<void(Timer*)> deleter = [stage_is_over](Timer* timer) {
        *stage_is_over = true;
        delete timer;
    };
    std::function<void()> timeout_handler = [match = shared_from_this(), stage_is_over]() {
        match->game_->HandleTimeout(stage_is_over.get());
    };
    if (kMinAlertSec > sec / 2) {
        tasks.emplace_front(sec, timeout_handler);
    } else {
        tasks.emplace_front(kMinAlertSec, timeout_handler);
        uint64_t sum_alert_sec = kMinAlertSec;
        for (uint64_t alert_sec = kMinAlertSec; sum_alert_sec < sec / 2; sum_alert_sec += alert_sec, alert_sec *= 2) {
            tasks.emplace_front(alert_sec, [this, alert_sec] {
                Boardcast() << "剩余时间" << (alert_sec / 60) << "分" << (alert_sec % 60) << "秒";
            });
        }
        tasks.emplace_front(sec - sum_alert_sec, [] {});
    }
    timer_ = std::unique_ptr<Timer, std::function<void(Timer*)>>(new Timer(std::move(tasks)), std::move(deleter));
}

void Match::StopTimer() { timer_ = nullptr; }

std::string Match::OptionInfo() const { return game_->OptionInfo(); }

bool Match::SatisfyMaxPlayer_() const
{
    // TODO: player_num_each_user_
    return game_handle_.max_player_ != 0 && ready_uid_set_.size() + com_num_ >= game_handle_.max_player_;
}
