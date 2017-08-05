#ifndef STADIUM_REFEREE_H_
#define STADIUM_REFEREE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "stadium/game_data.h"

namespace stadium {

class Referee {
 public:
  Referee();
  ~Referee();

  void Setup(const std::vector<std::string>& names, const Map* map);
  Move HandleMove(const Move& move, int punter_id);
  void Finish();

 private:
  std::vector<std::string> names_;

  DISALLOW_COPY_AND_ASSIGN(Referee);
};

}  // namespace stadium

#endif  // STADIUM_REFEREE_H_
