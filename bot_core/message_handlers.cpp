#include "message_handlers.h"

#include "bot_core.h"
#include "db_manager.h"
#include "log.h"
#include "match.h"
#include "util.h"
#include "utility/msg_checker.h"

static MetaCommand make_command(const char* const description, const auto& cb, auto&&... checkers)
{
    return MetaCommand(description, cb, std::move(checkers)...);
};

static ErrCode help(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply,
                    const std::vector<MetaCommand>& cmds, const std::string& type)
{
    auto sender = reply();
    sender << "[可使用的" << type << "指令]";
    int i = 0;
    for (const MetaCommand& cmd : cmds) {
        sender << "\n[" << (++i) << "] " << cmd.Info();
    }
    return EC_OK;
}

ErrCode HandleRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, MsgReader& reader,
                      const replier_t reply, const std::vector<MetaCommand>& cmds)
{
    reader.Reset();
    for (const MetaCommand& cmd : cmds) {
        const std::optional<ErrCode> errcode = cmd.CallIfValid(reader, bot, uid, gid, reply);
        if (errcode.has_value()) {
            return *errcode;
        }
    }
    return EC_REQUEST_NOT_FOUND;
}

static ErrCode show_gamelist(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid,
                             const replier_t reply)
{
    int i = 0;
    if (bot.game_handles().empty()) {
        reply() << "未载入任何游戏";
        return EC_OK;
    }
    auto sender = reply();
    sender << "游戏列表：";
    for (const auto& [name, info] : bot.game_handles()) {
        sender << "\n" << (++i) << ". " << name;
        if (!info->game_id().has_value()) {
            sender << "（试玩）";
        }
    }
    return EC_OK;
}

template <bool skip_config>
static ErrCode new_game(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply,
                        const std::string& gamename)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 创建失败：未知的游戏名，请通过\"#游戏列表\"查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    }
    return bot.match_manager().NewMatch(*it->second, uid, gid, skip_config, reply);
}

static ErrCode config_over(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply)
{
    return bot.match_manager().ConfigOver(uid, gid, reply);
}

static ErrCode set_com_num(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply, const uint64_t com_num)
{
    return bot.match_manager().SetComNum(uid, gid, reply, com_num);
}

static ErrCode start_game(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply)
{
    return bot.match_manager().StartGame(uid, gid, reply);
}

template <bool even_if_game_started>
static ErrCode leave(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply)
{
    return bot.match_manager().DeletePlayer(uid, gid, reply, even_if_game_started);
}

static ErrCode join_private(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid,
                            const replier_t reply, const MatchID match_id)
{
    if (gid.has_value()) {
        reply() << "[错误] 加入失败：请私信裁判加入私密游戏，或去掉比赛ID以加入当前房间游戏";
        return EC_MATCH_NEED_REQUEST_PRIVATE;
    }
    return bot.match_manager().AddPlayerToPrivateGame(match_id, uid, reply);
}

static ErrCode join_public(BotCtx& bot, const UserID uid, const std::optional<GroupID>& gid, const replier_t reply)
{
    if (!gid.has_value()) {
        reply() << "[错误] 加入失败：若要加入私密游戏，请指明比赛ID";
        return EC_MATCH_NEED_ID;
    }
    return bot.match_manager().AddPlayerToPublicGame(*gid, uid, reply);
}

static ErrCode show_private_matches(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                    const replier_t reply)
{
    uint64_t count = 0;
    auto sender = reply();
    bot.match_manager().ForEachMatch([&sender, &count](const std::shared_ptr<Match> match) {
        if (match->IsPrivate() && match->state() == Match::State::NOT_STARTED) {
            ++count;
            sender << match->game_handle().name_ << " - [房主ID] " << match->host_uid() << " - [比赛ID] "
                   << match->mid() << "\n";
        }
    });
    if (count == 0) {
        sender << "当前无未开始的私密比赛";
    } else {
        sender << "共" << count << "场";
    }
    return EC_OK;
}

static ErrCode show_match_status(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                 const replier_t reply)
{
    // TODO: make it thread safe
    std::shared_ptr<Match> match = bot.match_manager().GetMatch(uid, gid);
    if (!match && gid.has_value()) {
        match = bot.match_manager().GetMatch(*gid);
    }
    if (!match && gid.has_value()) {
        reply() << "[错误] 查看失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    }
    if (!match && !gid.has_value()) {
        reply() << "[错误] 查看失败：您未加入游戏";
        return EC_MATCH_USER_NOT_IN_MATCH;
    };
    auto sender = reply();
    sender << "游戏名称：" << match->game_handle().name_ << "\n";
    sender << "配置信息：" << match->OptionInfo() << "\n";
    sender << "游戏状态："
           << (match->state() == Match::State::IN_CONFIGURING
                       ? "配置中"
                       : match->state() == Match::State::NOT_STARTED ? "未开始" : "已开始")
           << "\n";
    sender << "房间号：";
    if (match->gid().has_value()) {
        sender << *gid << "\n";
    } else {
        sender << "私密游戏"
               << "\n";
    }
    sender << "最多可参加人数：";
    if (match->game_handle().max_player_ == 0) {
        sender << "无限制";
    } else {
        sender << match->game_handle().max_player_;
    }
    sender << "人"
           << "\n房主：";
    if (match->gid().has_value()) {
        sender << GroupUserMsg(match->host_uid(), *match->gid());
    } else {
        sender << UserMsg(match->host_uid());
    }
    if (match->state() == Match::State::IS_STARTED) {
        const auto num = match->PlayerNum();
        sender << "\n玩家列表：" << num << "人";
        for (uint64_t pid = 0; pid < num; ++pid) {
            sender << "\n" << pid << "号：";
            match->PrintPlayer<false>(sender, pid);
        }
    } else {
        const std::set<UserID>& ready_uid_set = match->ready_uid_set();
        sender << "\n当前报名玩家：" << ready_uid_set.size() << "人";
        for (const UserID uid : ready_uid_set) {
            sender << "\n";
            if (match->gid().has_value()) {
                sender << GroupUserMsg(uid, *match->gid());
            } else {
                sender << UserMsg(uid);
            }
        }
    }
    return EC_OK;
}

static ErrCode show_rule(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, const replier_t reply,
                         const std::string& gamename)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 查看失败：未知的游戏名，请通过\"#游戏列表\"查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    };
    auto sender = reply();
    sender << "最多可参加人数：";
    if (it->second->max_player_ == 0) {
        sender << "无限制";
    } else {
        sender << it->second->max_player_;
    }
    sender << "人\n";
    sender << "详细规则：\n";
    sender << it->second->rule_;
    return EC_OK;
}

#ifdef WITH_MYSQL
static ErrCode show_profile(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                            const replier_t reply)
{
    const std::unique_ptr<DBManager>& db_manager = DBManager::GetDBManager();
    if (db_manager == nullptr) {
        reply() << "[错误] 查看失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    db_manager->GetUserProfit(uid, reply);  // TODO: pass sender
    return EC_OK;
}
#endif

const std::vector<MetaCommand> meta_cmds = {
        // GAME INFO: can be executed at any time
        make_command(
                "查看帮助",
                [](BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, const replier_t reply) {
                    return help(bot, uid, gid, reply, meta_cmds, "元");
                },
                VoidChecker("#帮助")),
#ifdef WITH_MYSQL
        make_command("查看个人信息", show_profile, VoidChecker("#个人信息")),
#endif
        make_command("查看游戏列表", show_gamelist, VoidChecker("#游戏列表")),
        make_command("查看游戏规则（游戏名称可以通过\"#游戏列表\"查看）", show_rule, VoidChecker("#规则"),
                     AnyArg("游戏名称", "某游戏名")),
        make_command("查看当前所有未开始的私密比赛", show_private_matches, VoidChecker("#私密游戏列表")),
        make_command("查看已加入，或该房间正在进行的比赛信息", show_match_status, VoidChecker("#游戏信息")),

        // NEW GAME: can only be executed by host
        make_command("在当前房间建立公开游戏，或私信bot以建立私密游戏（游戏名称可以通过\"#游戏列表\"查看）",
                     new_game<true>, VoidChecker("#新游戏"), AnyArg("游戏名称", "某游戏名")),
        make_command("在当前房间建立公开游戏，或私信bot以建立私密游戏，并进行游戏参数的配置（游戏名称可以通过\"#"
                     "游戏列表\"查看）",
                     new_game<false>, VoidChecker("#配置新游戏"), AnyArg("游戏名称", "某游戏名")),
        make_command("完成游戏参数配置后，允许玩家进入房间", config_over, VoidChecker("#配置完成")),
        make_command("设置参与游戏的AI数量", set_com_num, VoidChecker("#电脑数量"), ArithChecker<uint32_t>(0, 12, "数量")),
        make_command("房主开始游戏", start_game, VoidChecker("#开始游戏")),

        // JOIN/LEAVE GAME: can only be executed by player
        make_command("加入当前房间的公开游戏", join_public, VoidChecker("#加入游戏")),
        make_command("私信bot以加入私密游戏（私密比赛编号可以通过\"#私密游戏列表\"查看）", join_private,
                     VoidChecker("#加入游戏"), BasicChecker<MatchID>("私密比赛编号")),
        make_command("在游戏开始前退出游戏", leave<false>, VoidChecker("#退出游戏")),
        make_command("可以在游戏进行中退出游戏，需注意退出后无法继续参与原游戏", leave<true>, VoidChecker("#强制退出游戏")),
};

#ifdef WITH_MYSQL
static ErrCode release_game(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                            const replier_t reply, const std::string& gamename)
{
    const auto it = bot.game_handles().find(gamename);
    if (it == bot.game_handles().end()) {
        reply() << "[错误] 发布失败：未知的游戏名，请通过\"#游戏列表\"查看游戏名称";
        return EC_REQUEST_UNKNOWN_GAME;
    }
    std::optional<uint64_t> game_id = it->second->game_id();
    if (game_id.has_value()) {
        reply() << "[错误] 发布失败：游戏已发布，ID为" << *game_id;
        return EC_GAME_ALREADY_RELEASE;
    }
    const std::unique_ptr<DBManager>& db_manager = DBManager::GetDBManager();
    if (db_manager == nullptr) {
        reply() << "[错误] 发布失败：未连接数据库";
        return EC_DB_NOT_CONNECTED;
    }
    if (!(game_id = db_manager->ReleaseGame(gamename)).has_value()) {
        reply() << "[错误] 发布失败：发布失败，请查看错误日志";
        return EC_DB_RELEASE_GAME_FAILED;
    }
    it->second->set_game_id(game_id);
    reply() << "发布成功，游戏\'" << gamename << "\'的ID为" << *game_id;
    return EC_OK;
}
#endif

static ErrCode interrupt_public(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                const replier_t reply)
{
    if (!gid.has_value()) {
        reply() << "[错误] 中断失败：需要在房间中使用该指令";
        return EC_MATCH_NEED_REQUEST_PUBLIC;
    }
    const auto match = bot.match_manager().GetMatch(*gid);
    if (!match) {
        reply() << "[错误] 中断失败：该房间未进行游戏";
        return EC_MATCH_GROUP_NOT_IN_MATCH;
    }
    const auto rc = bot.match_manager().DeleteMatch(match->mid());
    reply() << (rc == EC_OK ? "中断成功" : "[错误] 中断失败：未知错误");
    return rc;
}

static ErrCode interrupt_private(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
                                 const replier_t reply, const MatchID match_id)
{
    const auto match = bot.match_manager().GetMatch(match_id);
    if (!match) {
        reply() << "[错误] 中断失败：游戏ID不存在";
        return EC_MATCH_NOT_EXIST;
    }
    const auto rc = bot.match_manager().DeleteMatch(match_id);
    reply() << (rc == EC_OK ? "中断成功" : "[错误] 中断失败：未知错误");
    return rc;
}

const std::vector<MetaCommand> admin_cmds = {
        make_command(
                "查看帮助",
                [](BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, const replier_t reply) {
                    return help(bot, uid, gid, reply, admin_cmds, "管理");
                },
                VoidChecker("%帮助")),
#ifdef WITH_MYSQL
        make_command("发布游戏，写入游戏信息到数据库", release_game, VoidChecker("%发布游戏"),
                     AnyArg("游戏名称", "某游戏名")),
#endif
        make_command("强制中断公开比赛", interrupt_public, VoidChecker("%中断游戏")),
        make_command("强制中断私密比赛", interrupt_private, VoidChecker("%中断游戏"),
                     BasicChecker<MatchID>("私密比赛编号")),
};
