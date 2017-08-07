#ifndef STADIUM_PUNTER_H_
#define STADIUM_PUNTER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "stadium/game_data.h"

namespace stadium {

struct PunterInfo {
  std::string name;
  std::vector<River> futures;
};

class Punter {
 public:
  virtual ~Punter() {}

  virtual PunterInfo SetUp(const common::SetUpData& args) = 0;
  virtual base::Optional<Move> OnTurn(const std::vector<Move>& moves) = 0;

 protected:
  Punter() {}
};

}  // namespace stadium

#endif  // STADIUM_PUNTER_H_
