#ifndef COMMON_SCORER_H_
#define COMMON_SCORER_H_

#include <vector>

#include "common/game_data.h"
#include "common/scorer.pb.h"

namespace common {

class Scorer {
 public:
  explicit Scorer(ScorerProto* data);
  ~Scorer();

  void Initialize(size_t num_punters, const GameMap& game_map);
  void AddFuture(size_t punter_id, const std::vector<Future>& futures);

  int GetScore(size_t punter_id) const;
  void Claim(size_t punter_id, int site_id1, int site_id2);
  int TryClaim(size_t punter_id, int site_id1, int site_id2) const;
  bool IsConnected(size_t punter_id, int site_id1, int site_id2) const;

 private:
  mutable ScorerProto* data_;

  DISALLOW_COPY_AND_ASSIGN(Scorer);
};

}  // namespace common

#endif  // COMMON_SCORER_H_
