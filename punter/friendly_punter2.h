#ifndef PUNTER_FRIENDLY_PUNTER2_H_
#define PUNTER_FRIENDLY_PUNTER2_H_

#include "framework/simple_punter.h"

namespace punter {

class FriendlyPunter2 : public framework::SimplePunter {
 public:
  FriendlyPunter2();
  ~FriendlyPunter2() override;
  
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
  framework::GameMove TryReplace();
};
  

}  // namespace punter

#endif  // PUNTER_FRIENDLY_PUNTER2_H_

