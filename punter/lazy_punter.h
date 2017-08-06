#ifndef PUNTER_LAZY_PUNTER_H_
#define PUNTER_LAZY_PUNTER_H_

#include "framework/simple_punter.h"

namespace punter {

class LazyPunter : public framework::SimplePunter {
 public:
  LazyPunter();
  ~LazyPunter() override;
  
  // framework::SimplePunter:
  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  framework::GameMove Run() override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

 private:
  std::vector<double> ComputeReachability(
      int mineIndex,
      const std::vector<std::pair<int, int>>& remaining_edges,
      const std::vector<std::vector<int>>& adj);
  double Evaluate();
};
  

}  // namespace punter

#endif  // PUNTER_LAZY_PUNTER_H_
