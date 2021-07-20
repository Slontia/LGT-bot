
#include <regex>

#include "db_manager.h"
#include "log.h"
#include "match.h"
#include "util.h"
#include "utility/msg_sender.h"

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#define HINSTANCE void*
#define GetProcAddress dlsym
#define FreeLibrary dlclose
#else
static_assert(false, "Not support OS");
#endif

template <typename Sender>
class MsgSenderForGameImpl : public MsgSenderForGame
{
   public:
    MsgSenderForGameImpl(Match& match, Sender&& sender) : match_(match), sender_(std::forward<Sender>(sender)) {}
    virtual ~MsgSenderForGameImpl() {}

    virtual void String(const char* const str, const size_t len) { sender_ << std::string_view(str, len); }

    virtual void PlayerName(const uint64_t pid) { match_.PrintPlayer<false>(sender_, pid); }

    virtual void AtPlayer(const uint64_t pid) { match_.PrintPlayer<true>(sender_, pid); }

   private:
    Match& match_;
    Sender sender_;
};

static MsgSenderForGame* Boardcast(void* match_p)
{
    Match& match = *static_cast<Match*>(match_p);
    return new MsgSenderForGameImpl(match, match.Boardcast());
}

static MsgSenderForGame* Tell(void* match_p, const uint64_t pid)
{
    Match& match = *static_cast<Match*>(match_p);
    return new MsgSenderForGameImpl(match, match.Tell(pid));
}

static void DeleteMsgSender(MsgSenderForGame* const sender) { delete sender; }

static void MatchGamePrepare(void* match) { static_cast<Match*>(match)->GamePrepare(); }

static void MatchGameOver(void* match, const int64_t scores[])
{
    Match& match_ref = *static_cast<Match*>(match);
    match_ref.GameOver(scores);
    match_ref.match_manager().DeleteMatch(match_ref.mid());
}

static void StartTimer(void* match, const uint64_t sec) { static_cast<Match*>(match)->StartTimer(sec); }

static void StopTimer(void* match) { static_cast<Match*>(match)->StopTimer(); }

static void LoadGame(HINSTANCE mod, GameHandleMap& game_handles)
{
    if (!mod) {
#ifdef __linux__
        ErrorLog() << "Load mod failed: " << dlerror();
#else
        ErrorLog() << "Load mod failed";
#endif
        return;
    }

    typedef int(/*__cdecl*/ *Init)(const NEW_BOARDCAST_MSG_SENDER_CALLBACK, const NEW_TELL_MSG_SENDER_CALLBACK,
                                   const DELETE_GAME_MSG_SENDER_CALLBACK, const GAME_PREPARE_CALLBACK,
                                   const GAME_OVER_CALLBACK, const START_TIMER_CALLBACK, const STOP_TIMER_CALLBACK);
    typedef char*(/*__cdecl*/ *GameInfo)(uint64_t*, const char**);
    typedef GameBase*(/*__cdecl*/ *NewGame)(void* const match);
    typedef int(/*__cdecl*/ *DeleteGame)(GameBase* const);

    Init init = (Init)GetProcAddress(mod, "Init");
    GameInfo game_info = (GameInfo)GetProcAddress(mod, "GameInfo");
    NewGame new_game = (NewGame)GetProcAddress(mod, "NewGame");
    DeleteGame delete_game = (DeleteGame)GetProcAddress(mod, "DeleteGame");

    if (!init || !game_info || !new_game || !delete_game) {
        ErrorLog() << "Load failed: some interface not be defined." << std::boolalpha << static_cast<bool>(new_game);
        return;
    }

    if (!init(&Boardcast, &Tell, &DeleteMsgSender, &MatchGamePrepare, &MatchGameOver, &StartTimer, &StopTimer)) {
        ErrorLog() << "Load failed: init failed";
        return;
    }

    const char* rule = nullptr;
    uint64_t max_player = 0;
    char* name = game_info(&max_player, &rule);
    if (!name) {
        ErrorLog() << "Load failed: Cannot get game game";
        return;
    }
    std::optional<uint64_t> game_id;
#ifdef WITH_MYSQL
    if (const std::unique_ptr<DBManager>& db_manager = DBManager::GetDBManager(); db_manager != nullptr) {
        game_id = db_manager->GetGameIDWithName(name);
    }
#endif
    game_handles.emplace(name, std::make_unique<GameHandle>(game_id, name, max_player, rule, new_game, delete_game,
                                                            [mod] { FreeLibrary(mod); }));
    InfoLog() << "Loaded successfully!";
}

void BotCtx::LoadGameModules_(const char* const games_path)
{
#ifdef _WIN32
    WIN32_FIND_DATA file_data;
    HANDLE file_handle = FindFirstFile((std::string(games_path) + "\\*.dll").c_str(), &file_data);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        const auto dll_path = ".\\plugins\\" + std::string(file_data.cFileName);
        LoadGame(LoadLibrary(dll_path.c_str()), game_handles_);
    } while (FindNextFile(file_handle, &file_data));
    FindClose(file_handle);
    InfoLog() << "Load module count: " << game_handles_.size();
#elif __linux__
    DIR* d = opendir(games_path);
    if (!d) {
        ErrorLog() << "opendir failed";
        return;
    }
    const std::regex base_regex(".*so");
    std::smatch base_match;
    struct stat st;
    for (dirent* dp = nullptr; (dp = readdir(d)) != NULL;) {
        std::string name(dp->d_name);
        if (std::regex_match(name, base_match, base_regex); base_match.empty()) {
            DebugLog() << "Find irrelevant file " << name << ", skip";
            continue;
        }
        InfoLog() << "Loading library " << name;
        LoadGame(dlopen((std::string("./plugins/") + name).c_str(), RTLD_LAZY), game_handles_);
    }
    InfoLog() << "Loading finished.";
    closedir(d);
#endif
}
