#ifndef STADIUM_PUNTER_H_
#define STADIUM_PUNTER_H_

#include <string>
#include <vector>

#include "stadium/game_states.h"

namespace stadium {

class Punter {
 public:
  virtual ~Punter() {}

  virtual std::string Initialize(int punter_id,
                                 int num_punters,
                                 const Map& map) = 0;
  virtual Move OnTurn(const std::vector<Move>& moves) = 0;

 protected:
  Punter() {}
};

}  // namespace stadium

#endif  // STADIUM_PUNTER_H_
