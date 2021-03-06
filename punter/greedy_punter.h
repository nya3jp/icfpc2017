#ifndef PUNTER_GREEDY_PUNTER_H_
#define PUNTER_GREEDY_PUNTER_H_

#include <set>

#include "framework/simple_punter.h"

namespace punter {

class GreedyPunter : public framework::SimplePunter {
 public:
  GreedyPunter();
  ~GreedyPunter() override;

  void SetUp(const common::SetUpData& args) override;
  std::vector<framework::Future> GetFuturesImpl() override;
  framework::GameMove Run() override;
  void SetState(std::unique_ptr<base::Value> state) override;
  void ComputeLongestPath();

 private:
  std::vector<std::set<int>> connected_from_mine_; // mine_idx -> {site_id}
};

} // namespace punter

#endif  // PUNTER_GREEDY_PUNTER_H_
