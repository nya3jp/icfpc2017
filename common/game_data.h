#ifndef COMMON_GAME_DATA_H_
#define COMMON_GAME_DATA_H_

#include <vector>

#include "base/values.h"

namespace common {

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

struct GameMoves {
  static std::vector<GameMove> FromJson(const base::ListValue& value);
};

using Future = River;

struct Futures {
  static std::unique_ptr<base::Value> ToJson(
      const std::vector<Future>& futures);
};

}  // namespace common

#endif  // COMMON_GAME_DATA_H_
