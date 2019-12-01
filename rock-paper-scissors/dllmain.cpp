// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "dllmain.h"
#include "callbacks.h"

static bool g_initiated = false;

extern "C" void Init(const AtCallback at,
                     const PrivateResponseCallback pri_msg,
                     const PublicResponseCallback pub_msg,
                     const GameOverCallback game_over)
{
  at_cb = at;
  private_response_cb = pri_msg;
  public_response_cb = pub_msg;
  game_over_cb = game_over;
  g_initiated = true;
}

extern "C" Game* StartGame(void* match, const uint32_t player_num)
{
  return new Game(match, player_num);
}

extern "C" bool RequestGame(Game* const game, int64_t* qq, char* const msg)
{

}

extern "C" void GameInfo(char** const name, uint32_t* const min_player, uint32_t* const max_player)
{

}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

