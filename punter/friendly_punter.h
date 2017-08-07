#ifndef PUNTER_FRIENDLY_PUNTER_H_
#define PUNTER_FRIENDLY_PUNTER_H_

#include "framework/simple_punter.h"

namespace punter {

class FriendlyPunter : public framework::SimplePunter {
 public:
  FriendlyPunter();
  ~FriendlyPunter() override;
  
  // framework::SimplePunter:
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

#endif  // PUNTER_FRIENDLY_PUNTER_H_

