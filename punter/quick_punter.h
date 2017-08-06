#ifndef PUNTER_QUICK_PUNTER_H_
#define PUNTER_QUICK_PUNTER_H_

#include <set>
#include "framework/game.h"
#include "punter/quick_punter.pb.h"

namespace punter {

class QuickPunter : public framework::Punter {
 public:
  QuickPunter();
  ~QuickPunter() override;

  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  framework::GameMove Run(const std::vector<framework::GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

 private:
  QuickPunerProto proto_;
  std::set<int> sites_connected_to_mine_;
  
  DISALLOW_COPY_AND_ASSIGN(QuickPunter);
};

} // namespace punter

#endif  // PUNTER_QUICK_PUNTER_H_
