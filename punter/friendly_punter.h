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
  std::pair<int, int> FindForMine(int mine_index,
      const std::vector<std::vector<int>>& adj,
      const std::vector<std::vector<int>>& adj_available,
      const std::vector<std::vector<int>>& value,
      const std::vector<std::vector<bool>>& covered);
};
  

}  // namespace punter

#endif  // PUNTER_FRIENDLY_PUNTER_H_

