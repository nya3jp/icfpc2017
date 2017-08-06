#ifndef STADIUM_REFEREE_H_
#define STADIUM_REFEREE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "stadium/game_data.h"
#include "stadium/punter.h"
#include "common/scorer.pb.h"

namespace stadium {

class Referee {
 public:
  Referee();
  ~Referee();

  void Setup(const std::vector<PunterInfo>& punter_info_list, const Map* map);
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

  std::vector<int> ComputeScores() const;

  std::vector<PunterInfo> punter_info_list_;
  MapState map_state_;
  std::vector<Move> move_history_;

  mutable common::ScorerProto scorer_;
  DISALLOW_COPY_AND_ASSIGN(Referee);
};

}  // namespace stadium

#endif  // STADIUM_REFEREE_H_
