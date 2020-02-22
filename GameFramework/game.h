#pragma once
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include "msg_checker.h"
#include "game_base.h"
#include "game_stage.h"
#include "timer.h"

class Stage;

class Game : public GameBase
{
public:
  Game(void* const match, const uint64_t player_num, std::unique_ptr<Stage>&& main_stage, const std::function<int64_t(uint64_t)>& player_score);
  virtual ~Game() {}
  /* Return true when is_over_ switch from false to true */
  virtual void __cdecl HandleRequest(const uint64_t pid, const bool is_public, const char* const msg) override;
  std::unique_ptr<Timer, std::function<void(Timer*)>> Time(const uint64_t sec);
  void Help(const std::function<void(const std::string&)>& reply);
  //void Boardcast(const std::string& msg) { boardcast_f(match_, msg); }
  //void Tell(const uint64_t pid, const std::string& msg) { tell_f(match_, pid, msg); }
  //std::string At(const uint64_t pid) { return at_f(match_, pid); }

private:
  void OnGameOver();

  void* const match_;
  const uint64_t player_num_;
  const std::unique_ptr<Stage> main_stage_;
  const std::function<int64_t(uint64_t)> player_score_f_;
  bool is_over_;
  std::optional<std::vector<int64_t>> scores_;
  std::unique_ptr<Timer> timer_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> is_busy_;
  const std::shared_ptr<MsgCommand<void(const std::function<void(const std::string&)>)>> help_cmd_;
};
