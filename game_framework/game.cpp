#include <cassert>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include "game_stage.h"
#include "utility/msg_checker.h"
#include "game_base.h"
#include "game.h"

class Player;

Game::Game(void* const match)
  : match_(match), main_stage_(nullptr), is_over_(false),
  help_cmd_(MakeCommand<void(const replier_t)>("查看游戏帮助", BindThis(this, &Game::Help), VoidChecker("帮助")))
{}

bool Game::StartGame(const uint64_t player_num)
{
  assert(main_stage_ == nullptr);
  if (player_num >= k_min_player && (k_max_player == 0 || player_num <= k_max_player) && options_.IsValidPlayerNum(k_min_player))
  {
    player_num_ = player_num;
    main_stage_ = MakeMainStage(player_num, options_);
    main_stage_->Init(match_, std::bind(g_start_timer_cb, match_, std::placeholders::_1), std::bind(g_stop_timer_cb, match_));
    return true;
  }
  return false;
}

/* Return true when is_over_ switch from false to true */
void /*__cdecl*/ Game::HandleRequest(const uint64_t pid, const bool is_public, const char* const msg)
{
  using namespace std::string_literals;
  std::lock_guard<SpinLock> l(lock_);
  const replier_t reply = [this, pid, is_public]() -> MsgSenderWrapper
  {
    if (is_public)
    {
      auto sender = ::Boardcast(match_);
      sender << AtMsg(pid);
      return sender;
    }
    return ::Tell(match_, pid);
  }; // we must convert lambda to std::function to pass it into CallIfValid
  if (is_over_) { reply() << "[错误] 差一点点，游戏已经结束了哦~"; }
  else
  {
    assert(msg);
    MsgReader reader(msg);
    if (help_cmd_->CallIfValid(reader, std::tuple{ reply })) { return; }
    if (main_stage_)
    {
      if (!main_stage_->HandleRequest(reader, pid, is_public, reply)) { reply() << "[错误] 未预料的游戏请求"; }
      if (main_stage_->IsOver()) { OnGameOver(); }
    }
    else if (!options_.SetOption(reader)) { reply() << "[错误] 未预料的游戏设置"; }
    else { reply() << "设置成功！目前配置："s << OptionInfo(); }
  }
}

void Game::HandleTimeout(const bool* const stage_is_over)
{
  std::lock_guard<SpinLock> l(lock_);
  if (!*stage_is_over) { main_stage_->HandleTimeout(); }
}

void Game::Help(const replier_t reply)
{
  auto sender = reply();
  if (main_stage_)
  {
    sender << "\n[当前阶段]\n";
    main_stage_->StageInfo(sender);
    sender << "\n\n";
  }
  sender << "[当前可使用游戏命令]";
  sender << "\n[1] " << help_cmd_->Info();
  if (main_stage_)
  {
    main_stage_->CommandInfo(1, sender);
  }
  else
  {
    uint32_t i = 1;
    for (const std::string& option_info : options_.Infos()) { sender << "\n[" << (++i) << "] " << option_info; }
  }
}

const char* Game::OptionInfo() const
{
  thread_local static std::string s;
  s = options_.StatusInfo();
  return s.c_str();
}

void Game::OnGameOver()
{
  is_over_ = true;
  std::vector<int64_t> scores(player_num_);
  for (uint64_t pid = 0; pid < player_num_; ++pid)
  {
    scores[pid] = main_stage_->PlayerScore(pid);
  }
  g_game_over_cb(match_, scores.data());
}
