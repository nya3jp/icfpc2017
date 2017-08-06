#ifndef STADIUM_SCORER_H_
#define STADIUM_SCORER_H_

#include <vector>

#include "stadium/scorer.h"
#include "stadium/game_data.h"
#include "common/scorer.pb.h"

namespace stadium {

struct PunterInfo;

class Scorer {
 public:
  Scorer();
  ~Scorer();

  void Initialize(const std::vector<PunterInfo>& punter_info_list,
                  const Map& game_map);
  int GetScore(size_t punter_id) const;
  void Claim(size_t punter_id, int site_id1, int site_id2);

 private:
  class UnionFindSet;

  std::vector<int> site_id_list_;  // index -> site_id mapping.
  std::vector<int> mine_list_;

  mutable common::ScorerProto data_;

  DISALLOW_COPY_AND_ASSIGN(Scorer);
};

}  // namespace stadium

#endif  // STADIUM_SCORER_H_
