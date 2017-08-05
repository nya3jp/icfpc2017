#ifndef FRAMEWORK_GAME_H_
#define FRAMEWORK_GAME_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"

namespace framework {

struct Site {
  int id;

  static Site FromJson(const base::Value& value);
  static std::unique_ptr<base::Value> ToJson(const Site& site);
};

struct River {
  int source;
  int target;

  static River FromJson(const base::Value& value);
  static std::unique_ptr<base::Value> ToJson(const River& river);
};

struct GameMap {
  std::vector<Site> sites;
  std::vector<River> rivers;
  std::vector<int> mines;

  static GameMap FromJson(const base::Value& value);
  static std::unique_ptr<base::Value> ToJson(const GameMap& game_map);
};

struct GameMove {
  enum class Type {
    CLAIM, PASS,
  };

  Type type;
  int punter_id;
  int source;  // Available only when type is CLAIM.
  int target;  // Available only when type is CLAIM.

  static GameMove Pass(int punter_id);
  static GameMove Claim(int punter_id, int source, int target);

  static GameMove FromJson(const base::Value& value);
  static std::unique_ptr<base::Value> ToJson(const GameMove& game_move);
};

using Future = River;

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
  std::unique_ptr<Punter> punter_;

  DISALLOW_COPY_AND_ASSIGN(Game);
};

}  // namespace framework

#endif  // FRAMEWORK_GAME_H_
