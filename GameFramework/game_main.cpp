#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <iostream>
#include "game.h"
#include "dllmain.h"
#include "game_main.h"

typedef void (*request_f)(char const* msg);

std::function<void(void*, const std::string&)> boardcast_f;
std::function<void(void*, const uint64_t, const std::string&)> tell_f;
std::function<std::string(void*, const uint64_t)> at_f;
std::function<void(void*, const std::vector<int64_t>&)> game_over_f;
std::function<void(void*, const uint64_t)> start_timer_f;
std::function<void(void*)> stop_timer_f;

bool /*__cdecl*/ Init(const boardcast boardcast, const tell tell, const at at, const game_over game_over, const start_timer start_timer, const stop_timer stop_timer)
{
  boardcast_f = [boardcast](void* match, const std::string& msg) { boardcast(match, msg.c_str()); };
  tell_f = [tell](void* match, const uint64_t pid, const std::string& msg) { tell(match, pid, msg.c_str()); };
  at_f = [at](void* match, const uint64_t pid) { return at(match, pid); };
  game_over_f = [game_over](void* match, const std::vector<int64_t>& scores)
  {
    game_over(match, scores.empty() ? nullptr : scores.data());
  };
  start_timer_f = start_timer;
  stop_timer_f = stop_timer;
  return true;
}

const char* /*__cdecl*/ GameInfo(uint64_t* min_player, uint64_t* max_player, const char** rule)
{
  if (!min_player || !max_player) { return nullptr; }
  *min_player = k_min_player;
  *max_player = k_max_player;
  *rule = Rule();
  return k_game_name.c_str();
}

GameBase* /*__cdecl*/ NewGame(void* const match)
{
  Game* game = new Game(match);
  return game;
}

void /*__cdecl*/ DeleteGame(GameBase* const game)
{
  delete(game);
}

