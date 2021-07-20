#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <cassert>
#include <variant>

#include "bot_core.h"
#include "game_framework/game_main.h"
#include "utility/msg_sender.h"
#include "utility/spinlock.h"

using ModGuard = std::function<void()>;
using replier_t = std::function<MsgSenderWrapper<MsgSenderForBot>()>;

template <typename TRef, typename T>
concept UniRef = std::is_same_v<std::decay_t<TRef>, T>;

struct GameHandle {
    GameHandle(const std::optional<uint64_t> game_id, const std::string& name, const uint64_t max_player,
               const std::string& rule, const std::function<GameBase*(void* const)>& new_game,
               const std::function<void(GameBase* const)>& delete_game, ModGuard&& mod_guard)
            : is_released_(game_id.has_value()),
	      game_id_(game_id.has_value() ? *game_id : 0),
              name_(name),
              max_player_(max_player),
              rule_(rule),
              new_game_(new_game),
              delete_game_(delete_game),
              mod_guard_(std::forward<ModGuard>(mod_guard))
    {
    }
    GameHandle(GameHandle&&) = delete;

    std::optional<uint64_t> game_id() const
    {
        if (is_released_.load()) {
            return game_id_.load();
	} else {
            return std::nullopt;
	}
    }

    void set_game_id(const uint64_t game_id)
    {
        assert(!is_released_.load());
	game_id_.store(game_id);
	is_released_.store(true);
    }

    void clear_game_id()
    {
        assert(is_released_.load());
	is_released_.store(false);
	game_id_.store(0);
    }

    const std::string name_;
    const uint64_t max_player_;
    const std::string rule_;
    const std::function<GameBase*(void* const)> new_game_;
    const std::function<void(GameBase* const)> delete_game_;
    ModGuard mod_guard_;
 
  private:
    std::atomic<bool> is_released_;
    std::atomic<uint64_t> game_id_;
};

extern const int32_t LGT_AC;
extern const UserID INVALID_USER_ID;
extern const GroupID INVALID_GROUP_ID;

class Match;
class BotCtx;

class MatchManager
{
   public:
    MatchManager(BotCtx& bot) : bot_(bot), next_mid_(0) {}
    ErrCode NewMatch(const GameHandle& game_handle, const UserID uid, const std::optional<GroupID> gid,
                     const bool skip_config, const replier_t reply);
    ErrCode ConfigOver(const UserID uid, const std::optional<GroupID> gid, const replier_t reply);
    ErrCode SetComNum(const UserID uid, const std::optional<GroupID> gid, const replier_t reply, const uint64_t com_num);
    ErrCode StartGame(const UserID uid, const std::optional<GroupID> gid, const replier_t reply);
    ErrCode AddPlayerToPrivateGame(const MatchID mid, const UserID uid, const replier_t reply);
    ErrCode AddPlayerToPublicGame(const GroupID gid, const UserID uid, const replier_t reply);
    ErrCode DeletePlayer(const UserID uid, const std::optional<GroupID> gid, const replier_t reply, const bool force);
    ErrCode DeleteMatch(const MatchID id);
    std::shared_ptr<Match> GetMatch(const MatchID mid);
    std::shared_ptr<Match> GetMatch(const UserID uid, const std::optional<GroupID> gid);
    std::shared_ptr<Match> GetMatch(const GroupID gid);
    void ForEachMatch(const std::function<void(const std::shared_ptr<Match>)>);

   private:
    std::variant<ErrCode, std::shared_ptr<Match>> UnsafeGetMatchByHost_(
            const UserID uid, const std::optional<GroupID> gid, const replier_t reply);
    ErrCode AddPlayer_(const std::shared_ptr<Match>& match, const UserID, const replier_t reply);
    void DeleteMatch_(const MatchID id);
    template <typename IDType>
    std::shared_ptr<Match> GetMatch_(const IDType id, const std::map<IDType, std::shared_ptr<Match>>& id2match);
    template <typename IDType>
    void BindMatch_(const IDType id, std::map<IDType, std::shared_ptr<Match>>& id2match, std::shared_ptr<Match> match);
    template <typename IDType>
    void UnbindMatch_(const IDType id, std::map<IDType, std::shared_ptr<Match>>& id2match);
    MatchID NewMatchID_();

    BotCtx& bot_;
    std::map<MatchID, std::shared_ptr<Match>> mid2match_;
    std::map<UserID, std::shared_ptr<Match>> uid2match_;
    std::map<GroupID, std::shared_ptr<Match>> gid2match_;
    MatchID next_mid_;
    SpinLock spinlock_;
};

using GameHandleMap = std::map<std::string, std::unique_ptr<GameHandle>>;

class BotCtx
{
   public:
    BotCtx(const uint64_t this_uid, const NEW_MSG_SENDER_CALLBACK new_msg_sender_cb,
           const DELETE_MSG_SENDER_CALLBACK delete_msg_sender_cb, const char* const game_path,
           const uint64_t* const admins, const uint64_t admin_count)
            : this_uid_(this_uid),
              new_msg_sender_cb_(new_msg_sender_cb),
              delete_msg_sender_cb_(delete_msg_sender_cb),
              match_manager_(*this)
    {
        LoadGameModules_(game_path);
        LoadAdmins_(admins, admin_count);
    }

    MatchManager& match_manager() { return match_manager_; }
    const auto& game_handles() const { return game_handles_; }

    bool HasAdmin(const UserID uid) const { return admins_.find(uid) != admins_.end(); }

    auto ToUserRaw(const UserID uid) const
    {
        return std::unique_ptr<MsgSenderForBot, DELETE_MSG_SENDER_CALLBACK>(new_msg_sender_cb_(Target::TO_USER, uid),
                                                                            delete_msg_sender_cb_);
    }

    auto ToGroupRaw(const GroupID gid) const
    {
        return std::unique_ptr<MsgSenderForBot, DELETE_MSG_SENDER_CALLBACK>(new_msg_sender_cb_(Target::TO_GROUP, gid),
                                                                            delete_msg_sender_cb_);
    }

    auto ToUser(const UserID uid) const { return MsgSenderWrapper(ToUserRaw(uid)); }

    auto ToGroup(const GroupID gid) const { return MsgSenderWrapper(ToGroupRaw(gid)); }

   private:
    void LoadGameModules_(const char* const games_path);
    void LoadAdmins_(const uint64_t* const admins, const uint64_t admin_count);

    const UserID this_uid_;
    const NEW_MSG_SENDER_CALLBACK new_msg_sender_cb_;
    const DELETE_MSG_SENDER_CALLBACK delete_msg_sender_cb_;
    std::mutex mutex_;
    std::map<std::string, std::unique_ptr<GameHandle>> game_handles_;
    std::set<UserID> admins_;
    MatchManager match_manager_;
};
