#ifndef FRAMEWORK_GAME_H_
#define FRAMEWORK_GAME_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "common/game_data.h"

namespace framework {

using Site = common::Site;
using River = common::River;
using GameMap = common::GameMap;
using GameMove = common::GameMove;
using Future = common::Future;

class Punter {
 public:
  virtual ~Punter() = default;

  virtual void SetUp(
      int punter_id, int num_punters, const GameMap& game_map) = 0;
  virtual std::vector<Future> GetFutures() {
    return {};
  }

  virtual GameMove Run(const std::vector<GameMove>& moves) = 0;

  virtual void SetState(std::unique_ptr<base::Value> state) = 0;
  virtual std::unique_ptr<base::Value> GetState() = 0;

 protected:
  Punter() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Punter);
};

class Game {
 public:
  Game(std::unique_ptr<Punter> punter);
  ~Game();

  void Run();

 private:
  bool RunImpl();

  std::unique_ptr<Punter> punter_;

  DISALLOW_COPY_AND_ASSIGN(Game);
};

}  // namespace framework

#endif  // FRAMEWORK_GAME_H_
