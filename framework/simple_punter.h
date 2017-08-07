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
  virtual void SetStateFromProto(std::unique_ptr<GameStateProto> state);
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
    int river;  // This Edge's index in RiverProto.
  };

  bool CanSplurge() const {
    return can_splurge_;
  }
  bool CanOption() const {
    return can_option_;
  }

  int GetOptionsRemaining() const;
  int GetNumSplurgableEdges() const ;

  // Creates GameMove to return from Run(). Note that the arguments must be
  // site indexes.
  GameMove CreatePass() const;
  GameMove CreateClaim(int source_index, int target_index) const;
  // Note that this method empties route.
  GameMove CreateSplurge(std::vector<int>* route_in_index) const;
  GameMove CreateOption(int source_index, int target_index) const;

  int GetScore(int punter_id) const;
  int TryClaim(int punter_id, int site_index1, int site_index2) const;
  bool IsConnected(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> GetConnectedMineList(int punter_id, int site_index) const;
  std::vector<int> GetConnectedSiteList(int punter_id, int site_index) const;

  bool IsConnectable(int punter_id, int site_index1, int site_index2) const;
  std::vector<int> GetConnectableSiteTableFromSite(int punter_id,
                                                   int start_site) const;
  std::vector<int> Simulate(const std::vector<GameMove>& moves) const;

  // Returns: punter_id who claims the river between site_index1 and
  // site_index2, or -1 if not claimed, or -2 if not directly connected.
  int GetClaimingPunter(int site_index1, int site_index2) const;
  int GetOptioningPunter(int site_index1, int site_index2) const;

  // Converts index-based GameMove into site id based GameMove.
  void InternalGameMoveToExternal(GameMove* m) const;
  
  int num_punters_ = -1;
  int punter_id_ = -1;

  std::vector<std::vector<Edge>> edges_; // site_idx -> {Edge}

  mutable GameStateProto proto_;

  // Aliases to proto_.
  google::protobuf::RepeatedPtrField<RiverProto>* rivers_;
  google::protobuf::RepeatedPtrField<MineProto>* mines_;

  // river ids that were recently claimed / optioned
  std::vector<int> recent_updated_;

 private:
  void SetAliasesToProto();

  int FindSiteIdxFromSiteId(int id) const;

  void GenerateAdjacencyList();

  bool can_splurge_ = false;
  bool can_option_ = false;

  // Alias to proto_.
  google::protobuf::RepeatedPtrField<SiteProto>* sites_;

  DISALLOW_COPY_AND_ASSIGN(SimplePunter);
};

}  // namespace framework


#endif  // FRAMEWORK_SIMPLE_PUNTER_H_
