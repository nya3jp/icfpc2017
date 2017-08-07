#ifndef FRAMEWORK_GAME_H_
#define FRAMEWORK_GAME_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
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

  // Called for each run, before ping message exchanging.
  virtual void OnInit() {}

  virtual void SetUp(const common::SetUpData& args) = 0;
  virtual std::vector<Future> GetFutures() {
    return {};
  }
  virtual void EnableSplurges() {}
  virtual void EnableOptions() {}

  virtual GameMove Run(const std::vector<GameMove>& moves) = 0;
  virtual void OnFinish() {}
  virtual void SetState(std::unique_ptr<base::Value> state) = 0;
  virtual std::unique_ptr<base::Value> GetState() = 0;

  void SetEndTime(const base::TimeTicks& end_time) {
    end_time_ = end_time;
  }

  // Maybe non-positive, in case of timeout.
  base::TimeDelta approxy_remaining_time() {
    return end_time_ - base::TimeTicks::Now();
  }

 protected:
  Punter() = default;

 private:
  base::TimeTicks end_time_;
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
