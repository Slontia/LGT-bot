#pragma once
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include "game_stage.h"
#include "msg_checker.h"
#include "game_base.h"

class Player;
struct GameEnv;

static std::function<void(const uint64_t, const std::string&)> boardcast_f;
static std::function<void(const uint64_t, const uint64_t, const std::string&)> tell_f;
static std::function<std::string(const uint64_t, const uint64_t)> at_f;
static std::function<void(const uint64_t game_id, const std::vector<int64_t>& scores)> game_over_f;

template <typename StageEnum, typename GameEnv>
class Game : public GameBase
{
public:
  Game(const uint64_t mid, std::unique_ptr<GameEnv>&& game_env, std::unique_ptr<Stage<StageEnum, GameEnv>>&& main_stage)
    : mid_(mid), game_env_(std::move(game_env)), main_stage_(std::move(main_stage)), is_over_(false), timer_(nullptr), is_busy_(false) {}
  virtual ~Game() {}

  /* Return true when is_over_ switch from false to true */
  virtual void __cdecl HandleRequest(const uint64_t pid, const bool is_public, const char* const msg) override
  {
    
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [&is_busy = is_busy_]() { return !is_busy.exchange(true); });
    const auto reply = [this, pid, is_public](const std::string& msg) { is_public ? Boardcast(At(pid) + msg) : Tell(pid, msg); };
    if (is_over_)
    {
      assert(msg);
      MsgReader reader(msg);
      if (!main_stage_->HandleRequest(reader, pid, is_public, reply)) { reply("[����] δԤ�ϵ���Ϸ����"); }
      if (main_stage_->IsOver())
      {
        is_over_ = true;
        game_over_f(mid_, game_env_->PlayerScores());
      }
    }
    else { reply("[����] ��һ��㣬��Ϸ�Ѿ�������Ŷ~"); }
    is_busy_.store(false);
    lk.unlock();
    cv_.notify_one();
  }

  struct Deleter
  {
    Deleter(std::mutex& timer_mutex) : state_over_(std::make_shared<std::atomic<bool>>(false)) {}
    Deleter(const Deleter&) = delete;
    Deleter(Deleter&&) = default;
    ~Deleter() = default;
    void operator()(Timer* timer)
    {
      {
        std::lock_guard<std::mutex> lk(timer_mutex_);
        state_over_->store(true);
      }
      cv_.notify_one();
      delete timer;
    }
    std::shared_ptr<std::atomic<bool>> state_over_;
    std::mutex& timer_mutex_;
  };

  std::unique_ptr<Timer> Time(const uint64_t sec)
  {
    if (sec == 0) { return nullptr; }
    Deleter deleter;
    return std::unique_ptr<Timer>(new Timer(sec, [this, state_over = deleter.state_over_]()
    {
      std::unique_lock<std::mutex> lk(mutex_);
      cv_.wait(lk, [&state_over, &is_busy = is_busy_]() { return state_over.load() || !is_busy.exchange(true); });
      if (!state_over.load())
      {
        main_stage_->HandleTimeout();
        is_busy_.store(false);
      }
      lk.unlock();
      cv_.notify_one();
    }), std::move(deleter));
  }

  void Boardcast(const std::string& msg) { boardcast_f(mid_, msg); }
  void Tell(const uint64_t pid, const std::string& msg) { tell_f(mid_, pid, msg); }
  std::string At(const uint64_t pid) { return at_f(mid_, pid); }

private:
  const uint64_t mid_;
  const std::unique_ptr<GameEnv> game_env_;
  const std::unique_ptr<Stage<StageEnum, GameEnv>> main_stage_;
  bool is_over_;
  std::optional<std::vector<int64_t>> scores_;
  std::unique_ptr<Timer> timer_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> is_busy_;
};

