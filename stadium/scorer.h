#ifndef STADIUM_SCORER_H_
#define STADIUM_SCORER_H_

#include <vector>

#include "stadium/scorer.h"
#include "stadium/game_data.h"
#include "common/scorer.pb.h"

namespace stadium {

class Scorer {
 public:
  Scorer();
  ~Scorer();

  void Initialize(size_t num_punters, const Map& game_map);
  void AddFuture(size_t punter_id, const std::vector<common::Future>& futures);

  int GetScore(size_t punter_id) const;
  void Claim(size_t punter_id, int site_id1, int site_id2);

 private:
  mutable common::ScorerProto data_;

  DISALLOW_COPY_AND_ASSIGN(Scorer);
};

}  // namespace stadium

#endif  // STADIUM_SCORER_H_
