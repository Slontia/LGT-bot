#pragma once
#include <cassert>
#include <chrono>
#include <optional>
#include <variant>

#include "util.h"
#include "utility/msg_checker.h"
#include "utility/timer.h"

template <typename RetType>
using GameUserFuncType = RetType(const uint64_t, const bool, const replier_t);
template <typename RetType>
using GameCommand = Command<GameUserFuncType<RetType>>;

template <typename Stage, typename RetType, typename... Args, typename... Checkers>
static GameCommand<RetType> MakeStageCommand(Stage* stage, const char* const description,
                                                                 RetType (Stage::*cb)(Args...), Checkers&&... checkers)
{
    return GameCommand<RetType>(description, [stage, cb](Args... args) { return (stage->*cb)(std::forward<Args>(args)...); },
                                                  std::forward<Checkers>(checkers)...);
}

class StageBase
{
   public:
    enum class StageErrCode { OK, CHECKOUT, FAILED, NOT_FOUND, UNKNOWN };
    StageBase(std::string&& name)
            : name_(name.empty() ? "（匿名阶段）" : std::move(name)), match_(nullptr), is_over_(false)
    {
    }
    virtual ~StageBase() {}
    virtual void Init(void* const match, const std::function<void(const uint64_t)>& start_timer_f,
                      const std::function<void()>& stop_timer_f)
    {
        match_ = match;
        start_timer_f_ = start_timer_f;
        stop_timer_f_ = stop_timer_f;
    }
    virtual void HandleTimeout() = 0;
    virtual uint64_t CommandInfo(uint64_t i, MsgSenderWrapper<MsgSenderForGame>& sender) const = 0;
    virtual StageErrCode HandleRequest(MsgReader& reader, const uint64_t player_id, const bool is_public,
                                       const replier_t& reply) = 0;
    virtual void StageInfo(MsgSenderWrapper<MsgSenderForGame>& sender) const = 0;
    bool IsOver() const { return is_over_; }
    auto Boardcast() const { return ::Boardcast(match_); }
    auto Tell(const uint64_t pid) const { return ::Tell(match_, pid); }

   protected:
    template <typename Command>
    static uint64_t CommandInfo(uint64_t i, MsgSenderWrapper<MsgSenderForGame>& sender,
                                const Command& cmd)
    {
        sender << "\n[" << (++i) << "] " << cmd.Info();
        return i;
    }
    virtual void Over() { is_over_ = true; }
    const std::string name_;
    void* match_;
    std::function<void(const uint64_t)> start_timer_f_;
    std::function<void()> stop_timer_f_;

   private:
    bool is_over_;
};

class MainStageBase : public StageBase
{
   public:
    MainStageBase(std::string&& name) : StageBase(name.empty() ? "（主阶段）" : std::move(name)) {}
    virtual int64_t PlayerScore(const uint64_t pid) const = 0;
};

template <typename SubStage, typename RetType>
class SubStageCheckoutHelper
{
   public:
    virtual RetType NextSubStage(SubStage& sub_stage, const bool is_timeout) = 0;
};

template <bool IsMain, typename... SubStages>
class GameStage;

template <typename... SubStages>
using SubGameStage = GameStage<false, SubStages...>;
template <typename... SubStages>
using MainGameStage = GameStage<true, SubStages...>;

template <bool IsMain, typename SubStage, typename... SubStages>
class GameStage<IsMain, SubStage, SubStages...>
        : public std::conditional_t<IsMain, MainStageBase, StageBase>,
          public SubStageCheckoutHelper<SubStage,
                                        std::variant<std::unique_ptr<SubStage>, std::unique_ptr<SubStages>...>>,
          public SubStageCheckoutHelper<SubStages,
                                        std::variant<std::unique_ptr<SubStage>, std::unique_ptr<SubStages>...>>...
{
   private:
    using Base = std::conditional_t<IsMain, MainStageBase, StageBase>;

   protected:
    enum CompStageErrCode { OK, FAILED };

   public:
    using VariantSubStage = std::variant<std::unique_ptr<SubStage>, std::unique_ptr<SubStages>...>;
    using SubStageCheckoutHelper<SubStage, VariantSubStage>::NextSubStage;
    using SubStageCheckoutHelper<SubStages, VariantSubStage>::NextSubStage...;

    GameStage(std::string&& name = "",
              std::initializer_list<GameCommand<CompStageErrCode>>&& commands = {})
            : Base(std::move(name)), sub_stage_(), commands_(std::move(commands))
    {
    }

    virtual ~GameStage() {}

    virtual VariantSubStage OnStageBegin() = 0;

    virtual void Init(void* const match, const std::function<void(const uint64_t)>& start_timer_f,
                      const std::function<void()>& stop_timer_f)
    {
        StageBase::Init(match, start_timer_f, stop_timer_f);
        sub_stage_ = OnStageBegin();
        std::visit([match, start_timer_f,
                    stop_timer_f](auto&& sub_stage) { sub_stage->Init(match, start_timer_f, stop_timer_f); },
                   sub_stage_);
    }

    virtual StageBase::StageErrCode HandleRequest(MsgReader& reader, const uint64_t player_id, const bool is_public,
                                                  const replier_t& reply) override
    {
        for (const auto& cmd : commands_) {
            if (const auto rc = cmd.CallIfValid(reader, player_id, is_public, reply); rc.has_value()) {
                return ToStageErrCode(*rc);
            }
        }
        return std::visit(
                [this, &reader, player_id, is_public, &reply](auto&& sub_stage) {
                    const auto rc = sub_stage->HandleRequest(reader, player_id, is_public, reply);
                    if (sub_stage->IsOver()) {
                        this->CheckoutSubStage(false);
                    }
                    return rc;
                },
                sub_stage_);
    }

    virtual void HandleTimeout() override
    {
        std::visit(
                [this](auto&& sub_stage) {
                    sub_stage->HandleTimeout();
                    if (sub_stage->IsOver()) {
                        this->CheckoutSubStage(true);
                    }
                },
                sub_stage_);
    }

    virtual uint64_t CommandInfo(uint64_t i, MsgSenderWrapper<MsgSenderForGame>& sender) const override
    {
        for (const auto& cmd : commands_) {
            i = StageBase::CommandInfo(i, sender, cmd);
        }
        return std::visit([&i, &sender](auto&& sub_stage) { return sub_stage->CommandInfo(i, sender); }, sub_stage_);
    }

    virtual void StageInfo(MsgSenderWrapper<MsgSenderForGame>& sender) const override
    {
        sender << Base::name_ << " - ";
        std::visit([&sender](auto&& sub_stage) { sub_stage->StageInfo(sender); }, sub_stage_);
    }

    void CheckoutSubStage(const bool is_timeout)
    {
        // ensure previous substage is released before next substage built
        sub_stage_ = std::visit(
                [this, is_timeout](auto&& sub_stage) {
                    VariantSubStage new_sub_stage = NextSubStage(*sub_stage, is_timeout);
                    sub_stage = nullptr;
                    return std::move(new_sub_stage);
                },
                sub_stage_);
        std::visit(
                [this](auto&& sub_stage) {
                    if (!sub_stage) {
                        Over();
                    }  // no more substages
                    else {
                        sub_stage->Init(Base::match_, Base::start_timer_f_, Base::stop_timer_f_);
                    }
                },
                sub_stage_);
    }

   private:
    using StageBase::Over;
    static StageBase::StageErrCode ToStageErrCode(const CompStageErrCode rc)
    {
        switch (rc) {
        case OK:
            return StageBase::StageErrCode::OK;
        case FAILED:
            return StageBase::StageErrCode::FAILED;
        default:
            assert(false);
            return MainStageBase::StageErrCode::UNKNOWN;
        }
        // no more error code
    }
    VariantSubStage sub_stage_;
    std::vector<GameCommand<CompStageErrCode>> commands_;
};

template <bool IsMain>
class GameStage<IsMain> : public std::conditional_t<IsMain, MainStageBase, StageBase>
{
   private:
    using Base = std::conditional_t<IsMain, MainStageBase, StageBase>;

   protected:
    enum AtomStageErrCode { OK, CHECKOUT, FAILED };

   public:
    GameStage(std::string&& name, std::initializer_list<GameCommand<AtomStageErrCode>>&& commands)
            : Base(std::move(name)), timer_(nullptr), commands_(std::move(commands))
    {
    }
    virtual ~GameStage() { Base::stop_timer_f_(); }
    virtual void OnStageBegin() {}
    virtual void Init(void* const match, const std::function<void(const uint64_t)>& start_timer_f,
                      const std::function<void()>& stop_timer_f)
    {
        StageBase::Init(match, start_timer_f, stop_timer_f);
        OnStageBegin();
    }
    virtual void HandleTimeout() override final { StageBase::Over(); }
    virtual StageBase::StageErrCode HandleRequest(MsgReader& reader, const uint64_t player_id, const bool is_public,
                                                  const replier_t& reply) override
    {
        for (const auto& cmd : commands_) {
            if (const auto rc = cmd.CallIfValid(reader, player_id, is_public, reply); rc.has_value()) {
                if (*rc == CHECKOUT) {
                    Over();
                }
                return ToStageErrCode(*rc);
            }
        }
        return StageBase::StageErrCode::NOT_FOUND;
    }
    virtual uint64_t CommandInfo(uint64_t i, MsgSenderWrapper<MsgSenderForGame>& sender) const override
    {
        for (const auto& cmd : commands_) {
            i = StageBase::CommandInfo(i, sender, cmd);
        }
        return i;
    }
    virtual void StageInfo(MsgSenderWrapper<MsgSenderForGame>& sender) const override
    {
        sender << Base::name_;
        if (finish_time_.has_value()) {
            sender << " 剩余时间："
                   << std::chrono::duration_cast<std::chrono::seconds>(*finish_time_ - std::chrono::steady_clock::now())
                              .count()
                   << "秒";
        }
    }

   protected:
    void StartTimer(const uint64_t sec)
    {
        finish_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(sec);
        Base::start_timer_f_(sec);
    }

    void StopTimer()
    {
        Base::stop_timer_f();
        finish_time_ = nullptr;
    }

   private:
    static StageBase::StageErrCode ToStageErrCode(const AtomStageErrCode rc)
    {
        switch (rc) {
        case OK:
            return StageBase::StageErrCode::OK;
        case CHECKOUT:
            return StageBase::StageErrCode::CHECKOUT;
        case FAILED:
            return StageBase::StageErrCode::FAILED;
        default:
            assert(false);
            return MainStageBase::StageErrCode::UNKNOWN;
        }
        // no more error code
    }
    virtual void Over() override final
    {
        timer_ = nullptr;
        StageBase::Over();
    }
    std::unique_ptr<Timer, std::function<void(Timer*)>> timer_;
    // if command return true, the stage will be over
    std::vector<GameCommand<AtomStageErrCode>> commands_;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> finish_time_;
};
