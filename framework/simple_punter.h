#ifndef FRAMEWORK_SIMPLE_PUNTER_H_
#define FRAMEWORK_SIMPLE_PUNTER_H_

#include "base/macros.h"
#include "framework/game.h"
#include "framework/game_proto.pb.h"
#include "google/protobuf/repeated_field.h"

namespace framework {

class SimplePunter : public Punter {
 public:
  SimplePunter();
  ~SimplePunter() override;

  virtual GameMove Run() = 0;
  virtual std::vector<Future> GetFuturesImpl() { return {}; };

  // Punter overrides
  void SetUp(int punter_id, int num_punters, const GameMap& game_map) override;
  GameMove Run(const std::vector<GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;
  std::vector<Future> GetFutures() override final;

  int num_sites() const { return sites_->size(); }
  int dist_to_mine(int site, int mine) const {
    return sites_->Get(site).to_mine(mine).distance();
  }
  int FindSiteIdxFromSiteId(int id) const;
  void SaveToProto();
  void GenerateAdjacencyList();
  void GenerateSiteIdToSiteIndex();
  void ComputeDistanceToMine();

 protected:
  struct Edge {
    int site;  // site_index
    int river;  // Index to RiverProto.
  };

  int GetScore(int punter_id) const;
  int TryClaim(int punter_id, int site_index1, int site_index2) const;
  bool IsConnected(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> GetConnectedMineList(int punter_id, int site_index) const;
  std::vector<int> GetConnectedSiteList(int punter_id, int site_index) const;

  bool IsConnectable(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> Simulate(const std::vector<GameMove>& moves) const;

  int num_punters_ = -1;
  int punter_id_ = -1;

  std::vector<std::vector<Edge>> edges_; // site_idx -> {Edge}

  mutable GameStateProto proto_;
  google::protobuf::RepeatedPtrField<RiverProto>* rivers_;
  google::protobuf::RepeatedPtrField<MineProto>* mines_;

 private:
  void set_dist_to_mine(int site, int mine, int dist) const {
    return sites_->Mutable(site)->mutable_to_mine(mine)->set_distance(dist);
  }

  google::protobuf::RepeatedPtrField<SiteProto>* sites_;
  DISALLOW_COPY_AND_ASSIGN(SimplePunter);
};

}  // namespace framework


#endif  // FRAMEWORK_SIMPLE_PUNTER_H_
