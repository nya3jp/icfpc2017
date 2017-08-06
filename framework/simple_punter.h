#ifndef FRAMEWORK_SIMPLE_PUNTER_H_
#define FRAMEWORK_SIMPLE_PUNTER_H_

#include "base/macros.h"
#include "framework/game.h"
#include "framework/game_proto.pb.h"

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

  size_t num_sites() const { return sites_.size(); }
  int FindSiteIdxFromSiteId(int id);
  void SaveToProto();
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
  struct Edge {
    int site;
    int river;
  };

  int num_punters_ = -1;
  int punter_id_ = -1;
  std::vector<RiverWithPunter> rivers_;
  std::vector<int> mines_;

  std::vector<std::vector<Edge>> edges_; // site_idx -> {Edge}
  std::vector<std::vector<int>> dist_to_mine_; // site_idx -> mine_idx -> distance

  GameStateProto proto_;

 private:
  std::vector<Site> sites_;
  DISALLOW_COPY_AND_ASSIGN(SimplePunter);
};

}  // namespace framework


#endif  // FRAMEWORK_SIMPLE_PUNTER_H_
