#ifndef PUNTER_GREEDY_PUNTER_H_
#define PUNTER_GREEDY_PUNTER_H_

#include <set>

#include "framework/simple_punter.h"

namespace punter {

class GreedyPunter : public framework::SimplePunter {
 public:
  GreedyPunter();
  ~GreedyPunter() override;

  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  std::vector<framework::Future> GetFutures() override;
  framework::GameMove Run() override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;
  void ComputeLongestPath();

 private:
  std::vector<std::set<int>> connected_from_mine_; // mine_idx -> {site_id}
  std::vector<int> longest_path_;
  int longest_path_index_ = 0;
};

} // namespace punter

#endif  // PUNTER_GREEDY_PUNTER_H_
