#ifndef STADIUM_PUNTER_H_
#define STADIUM_PUNTER_H_

#include <string>
#include <vector>

#include "stadium/game_data.h"
#include "stadium/settings.h"

namespace stadium {

struct PunterInfo {
  std::string name;
  std::vector<River> futures;
};

class Punter {
 public:
  virtual ~Punter() {}

  virtual PunterInfo Setup(int punter_id,
                           int num_punters,
                           const Map* map,
                           const Settings& settings) = 0;
  virtual Move OnTurn(const std::vector<Move>& moves) = 0;

 protected:
  Punter() {}
};

}  // namespace stadium

#endif  // STADIUM_PUNTER_H_
