#pragma once
#include <stdint.h>
#include "game_base.h"

extern "C"
{
  typedef void (*boardcast)(void* match, const char* const msg);
  typedef void (*tell)(void* match, const uint64_t pid, const char* const msg);
  typedef const char* (*at)(void* match, const uint64_t pid);
  typedef void (*game_over)(void* match, const int64_t scores[]);
  typedef void (*start_timer)(void* match, const uint64_t sec);
  typedef void (*stop_timer)(void* match);
 
  __declspec(dllexport) bool __cdecl Init(const boardcast boardcast, const tell tell, const at at, const game_over game_over, const start_timer, const stop_timer);
  __declspec(dllexport) const char* __cdecl GameInfo(uint64_t* min_player, uint64_t* max_player, const char** rule);
  __declspec(dllexport) GameBase* __cdecl NewGame(void* const match);
  /* game should be delete in game dll */
  __declspec(dllexport) void __cdecl DeleteGame(GameBase* const game);

}