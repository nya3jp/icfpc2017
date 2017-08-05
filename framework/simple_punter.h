#ifndef FRAMEWORK_SIMPLE_PUNTER_H_
#define FRAMEWORK_SIMPLE_PUNTER_H_

#include "base/macros.h"
#include "framework/game.h"

namespace framework {

class SimplePunter : public Punter {
 public:
  SimplePunter();
  ~SimplePunter() override;

  virtual GameMove Run() = 0;

  // Punter overrides
  void SetUp(int punter_id, int num_punters, const GameMap& game_map) override;
  GameMove Run(const std::vector<GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

  void GenerateAdjacencyList();
  void GenerateSiteIdToSiteIndex();
  void ComputeDistanceToMine();

 protected:
  struct RiverWithPunter {
    RiverWithPunter() = default;
    RiverWithPunter(const River& r)
      : source(r.source),
        target(r.target),
        punter(-1) {
    }

    int source;
    int target;
    int punter;
  };

  int num_punters_ = -1;
  int punter_id_ = -1;
  std::vector<Site> sites_;
  std::vector<RiverWithPunter> rivers_;
  std::vector<int> mines_;

  std::map<int, int> site_id_to_site_idx_;
  std::vector<std::vector<int>> edges_;  // site_id -> {site_id}
  std::vector<std::vector<int>> dist_to_mine_; // site_idx -> mine_idx -> distance

 private:
  DISALLOW_COPY_AND_ASSIGN(SimplePunter);
};

}  // namespace framework


#endif  // FRAMEWORK_SIMPLE_PUNTER_H_
