#pragma once
#include "bot_core/bot_core.h"

class GameBase
{
public:
  virtual ~GameBase() = default;
  virtual bool /*__cdecl*/ StartGame(const bool is_public, const uint64_t player_num) = 0;
  virtual ErrCode /*__cdecl*/ HandleRequest(const uint64_t player_id, const bool is_public, const char* const msg) = 0;
  virtual void /*__cdecl*/ HandleTimeout(const bool* const stage_is_over) = 0;
  virtual const char* /*__cdecl*/ OptionInfo() const = 0;
};
