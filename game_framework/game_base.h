#pragma once
#include "bot_core/bot_core.h"

class GameBase
{
   public:
    virtual ~GameBase() = default;
    virtual bool /*__cdecl*/ StartGame(const bool is_public, const uint64_t user_num, const uint64_t com_num) = 0;
    virtual ErrCode /*__cdecl*/ HandleRequest(const uint64_t pid, const bool is_public,
                                              const char* const msg) = 0;
    virtual ErrCode /*__cdecl*/ HandleLeave(const uint64_t pid, const bool is_public) = 0;
    virtual ErrCode /*__cdecl*/ HandleTimeout(const bool* const stage_is_over) = 0;
    virtual const char* /*__cdecl*/ OptionInfo() const = 0;
};
