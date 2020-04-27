#pragma once
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include "game_stage.h"
#include "msg_checker.h"
#include "game_base.h"
#include "game.h"

class Player;

Game::Game(void* const match)
  : match_(match), main_stage_(nullptr), is_over_(false),
  help_cmd_(MakeCommand<void(const std::function<void(const std::string&)>)>("�鿴��Ϸ����", BindThis(this, &Game::Help), std::make_unique<VoidChecker>("����")))
{}

bool Game::StartGame(const uint64_t player_num)
{
  assert(main_stage_ == nullptr);
  if (player_num >= k_min_player && (k_max_player == 0 || player_num <= k_max_player) && options_.IsValidPlayerNum(k_min_player))
  {
    std::tie(main_stage_, player_score_f_) = MakeMainStage(player_num, options_);
    main_stage_->Init(match_, std::bind(start_timer_f, match_, std::placeholders::_1), std::bind(stop_timer_f, match_));
    return true;
  }
  return false;
}

/* Return true when is_over_ switch from false to true */
void __cdecl Game::HandleRequest(const uint64_t pid, const bool is_public, const char* const msg)
{
  std::lock_guard<SpinLock> l(lock_);
  const auto reply = [this, pid, is_public](const std::string& msg) { is_public ? boardcast_f(match_, at_f(match_, pid) + msg) : tell_f(match_, pid, msg); };
  if (is_over_) { reply("[����] ��һ��㣬��Ϸ�Ѿ�������Ŷ~"); }
  else
  {
    assert(msg);
    MsgReader reader(msg);
    if (help_cmd_->CallIfValid(reader, std::tuple{ reply })) { return; }
    if (main_stage_)
    {
      if (!main_stage_->HandleRequest(reader, pid, is_public, reply)) { reply("[����] δԤ�ϵ���Ϸ����"); }
      if (main_stage_->IsOver()) { OnGameOver(); }
    }
    else if (!options_.SetOption(reader)) { reply("[����] δԤ�ϵ���Ϸ����"); }
    else { reply("���óɹ���"); }
  }
}

void Game::HandleTimeout(const bool* const stage_is_over)
{
  std::lock_guard<SpinLock> l(lock_);
  if (!*stage_is_over) { main_stage_->HandleTimeout(); }
}

void Game::Help(const std::function<void(const std::string&)>& reply)
{
  std::stringstream ss;
  ss << "[��ǰ��ʹ����Ϸ����]";
  ss << std::endl << "[1] " << help_cmd_->Info();
  if (main_stage_) { main_stage_->CommandInfo(1, ss); }
  else
  {
    uint32_t i = 1;
    for (const std::string& option_info : options_.Infos()) { ss << std::endl << "[" << (++i) << "]" << option_info; }
  }
  reply(ss.str());
}

void Game::OnGameOver()
{
  is_over_ = true;
  std::vector<int64_t> scores(player_num_);
  for (uint64_t pid = 0; pid < player_num_; ++pid)
  {
    scores[pid] = player_score_f_(pid);
  }
  game_over_f(match_, scores);
}
