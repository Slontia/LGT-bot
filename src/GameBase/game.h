#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include <utility>

#include "time_trigger.h"
#include "game_stage.h"

class GamePlayer;
class Match;
class GameStage;
class StageContainer;
class Game;

typedef std::unique_ptr<GameStage> StagePtr;
typedef std::function<StagePtr(const std::string&, GameStage&)> StageCreator;
typedef std::map<StageId, StageCreator> StageCreatorMap;

class StageContainer
{
private:
  Game& game_;
  const std::map<StageId, StageCreator>   stage_creators_;
public:
  /* Constructor
  */
  StageContainer(Game& game, StageCreatorMap&& stage_creators);

  /* Returns game Stage pointer with id
  * if the id is main Stage id, returns the main game Stage pointer
  */
  StagePtr Make(const StageId& id, GameStage& father_stage) const;

  template <class Stage>
  StageCreator get_creator()
  {
    return [this](const std::string& name, GameStage& father_stage) -> StagePtr
    {
      /* Cast GameStage& to FatherStage&. */
      return std::make_unique<Stage>(game_, name, father_stage);
    };
  }
};

//template <class MainStage, class MyStageContainer>
class Game
{
public:
  typedef std::unique_ptr<GameStage> StagePtr;
  const std::string                   kGameId;
  const uint32_t                           kMinPlayer = 2;
  const uint32_t                           kMaxPlayer = 0;	// set 0 if no upper limit
  StageContainer                      stage_container_;
  StagePtr  main_stage_;
  TimeTrigger timer_;

  Game(
    Match& match,
    const std::string& game_id,
    const uint32_t& min_player,
    const uint32_t& max_player,
    StagePtr&& main_stage,
    StageCreatorMap&& stage_creators);

  virtual ~Game();

  inline bool valid_pnum(const uint32_t& pnum) const;

  /* send msg to all player */
  std::function<void(const std::string&)> broadcast_callback;
  void Broadcast(const std::string& msg) const;

  /* add new player */
  int32_t Join(std::shared_ptr<GamePlayer> player);

  /* create main_state_ */
  void StartGame();

  /* a callback function which write results to the database */
  std::function<void()> finish_game_callback;
  bool FinishGame();

  /* transmit msg to main_state_ */
  void Request(const uint32_t& pid, MessageIterator& msg);

  std::vector<std::shared_ptr<GamePlayer>>             players_;

private:
  Match&                              match_;
  
};


