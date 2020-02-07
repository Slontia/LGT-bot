
#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <iostream>
#include "game.h"
#include "dllmain.h"
#include "mygame.h"

typedef void (*request_f)(char const* msg);

bool __cdecl Init(const boardcast boardcast, const tell tell, const at at, const game_over game_over)
{
  boardcast_f = [boardcast](void* match, const std::string& msg) { boardcast(match, msg.c_str()); };
  tell_f = [tell](void* match, const uint64_t pid, const std::string& msg) { tell(match, pid, msg.c_str()); };
  at_f = [at](void* match, const uint64_t pid)
  {
    char buf[128] = { 0 };
    at(match, pid, buf, 128);
    return std::string(buf);
  };
  game_over_f = [game_over](void* match, const std::vector<int64_t>& scores) { game_over(match, scores.data()); };
  return true;
}

const char* __cdecl GameInfo(uint64_t* min_player, uint64_t* max_player)
{
  if (!min_player || !max_player) { return nullptr; }
  *min_player = k_min_player;
  *max_player = k_max_player;
  return k_game_name.c_str();
}

GameBase* __cdecl NewGame(void* const match, const uint64_t player_num)
{
  if (player_num < k_min_player || (player_num > k_max_player && k_max_player != 0)) { return nullptr; }
  Game<StageEnum, GameEnv>* game = new Game<StageEnum, GameEnv>(match, MakeGameEnv(player_num));
  game->SetMainStage(MakeMainStage(*game));
  return game;
}

void __cdecl DeleteGame(GameBase* const game)
{
  delete(game);
}
