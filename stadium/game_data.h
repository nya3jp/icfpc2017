#ifndef STADIUM_GAME_H_
#define STADIUM_GAME_H_

#include <vector>

#include "base/macros.h"
#include "base/values.h"

namespace stadium {

struct Site {
  int id;
  bool is_mine;
};

struct River {
  int source;
  int target;
};

struct Map {
  std::vector<Site> sites;
  std::vector<River> rivers;
  std::unique_ptr<base::Value> raw_value;

  static Map ReadFromFileOrDie(const std::string& path);
};

struct Move {
  enum class Type {
    CLAIM, PASS,
  };

  Type type;
  int punter_id;
  int source;  // Available only when type is CLAIM.
  int target;  // Available only when type is CLAIM.

  static Move MakeClaim(int punter_id, int source, int target) {
    return Move{Type::CLAIM, punter_id, source, target};
  }
  static Move MakePass(int punter_id) {
    return Move{Type::PASS, punter_id, -1, -1};
  }
};

}  // namespace stadium

#endif  // STADIUM_GAME_H_
