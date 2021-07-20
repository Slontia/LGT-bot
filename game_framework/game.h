#pragma once
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "game_base.h"
#include "game_options.h"
#include "game_stage.h"
#include "utility/msg_checker.h"
#include "utility/spinlock.h"
#include "utility/timer.h"

class StageBase;

class Game : public GameBase
{
   public:
    Game(void* const match);
    virtual ~Game() {}
    /* Return true when is_over_ switch from false to true */
    virtual bool /*__cdecl*/ StartGame(const bool is_public, const uint64_t user_num,const uint64_t com_num) override;
    virtual ErrCode /*__cdecl*/ HandleRequest(const uint64_t pid, const bool is_public, const char* const msg) override;
    virtual ErrCode /*__cdecl*/ HandleLeave(const uint64_t pid, const bool is_public) override;
    virtual ErrCode /*__cdecl*/ HandleTimeout(const bool* const stage_is_over) override;
    virtual const char* /*__cdecl*/ OptionInfo() const override;

   private:
    void Help_(const replier_t& reply);
    MsgSenderWrapper<MsgSenderForGame> Reply_(const uint64_t pid, const bool is_public);
    template <typename SenderRef>
    requires std::is_same_v<std::decay_t<SenderRef>, MsgSenderWrapper> void HelpInternal_(SenderRef&& sender);
    void OnGameOver_();

    void* const match_;
    std::unique_ptr<MainStageBase> main_stage_;
    bool is_over_;
    uint64_t user_controlled_num_;
    uint64_t com_num_;
    std::optional<std::vector<int64_t>> scores_;
    std::mutex lock_;
    const Command<void(const replier_t&)> help_cmd_;
    GameOption options_;
};
