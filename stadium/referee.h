#ifndef STADIUM_REFEREE_H_
#define STADIUM_REFEREE_H_

#include <map>
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
  Move HandleMove(int turn_id, int punter_id, const Move& move);
  void Finish();

 private:
  struct SiteState;
  struct RiverKey;
  struct RiverState;
  struct MapState {
    std::map<int, SiteState> sites;
    std::map<RiverKey, RiverState> rivers;

    static MapState FromMap(const Map& map);
  };

  std::vector<std::string> names_;
  MapState map_state_;

  DISALLOW_COPY_AND_ASSIGN(Referee);
};

}  // namespace stadium

#endif  // STADIUM_REFEREE_H_
