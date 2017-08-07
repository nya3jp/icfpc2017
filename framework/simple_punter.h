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

  // To be implemented by sub classes.
  virtual GameMove Run() = 0;
  virtual std::vector<Future> GetFuturesImpl() { return {}; };

  // Punter overrides

  void SetUp(const common::SetUpData& args) override;
  GameMove Run(const std::vector<GameMove>& moves) override;

  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

  std::vector<Future> GetFutures() override final;
  void EnableSplurges() override final;
  void EnableOptions() override final;

  // API for sub classes.
  int num_sites() const { return sites_->size(); }
  int dist_to_mine(int site, int mine) const;

 protected:
  struct Edge {
    int site;  // site_index
    int river;  // Index to RiverProto.
  };

  bool CanSplurge() const {
    return can_splurge_;
  }
  bool CanOption() const {
    return can_option_;
  }

  GameMove CreatePass() const;
  GameMove CreateClaim(int source, int target) const;
  GameMove CreateSplurge(std::vector<int>* route) const;
  GameMove CreateOption(int source, int target) const;

  int GetScore(int punter_id) const;
  int TryClaim(int punter_id, int site_index1, int site_index2) const;
  bool IsConnected(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> GetConnectedMineList(int punter_id, int site_index) const;
  std::vector<int> GetConnectedSiteList(int punter_id, int site_index) const;

  bool IsConnectable(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> Simulate(const std::vector<GameMove>& moves) const;

  // Returns: punter_id who claims the river between site_index1 and
  // site_index2, or -1 if not claimed, or -2 if not directly connected.
  int GetClaimingPunter(int site_index1, int site_index2) const;
  int GetOptioningPunter(int site_index1, int site_index2) const;

  int num_punters_ = -1;
  int punter_id_ = -1;

  std::vector<std::vector<Edge>> edges_; // site_idx -> {Edge}

  mutable GameStateProto proto_;
  google::protobuf::RepeatedPtrField<RiverProto>* rivers_;
  google::protobuf::RepeatedPtrField<MineProto>* mines_;

 private:
  int FindSiteIdxFromSiteId(int id) const;

  void GenerateAdjacencyList();

  bool can_splurge_ = false;
  bool can_option_ = false;

  google::protobuf::RepeatedPtrField<SiteProto>* sites_;

  DISALLOW_COPY_AND_ASSIGN(SimplePunter);
};

}  // namespace framework


#endif  // FRAMEWORK_SIMPLE_PUNTER_H_
