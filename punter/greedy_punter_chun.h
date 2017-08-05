#ifndef PUNTER_GREEDY_PUNTER_CHUN_H_
#define PUNTER_GREEDY_PUNTER_CHUN_H_

#include "framework/simple_punter.h"
#include "punter/gamemapforai.h"

namespace punter {

class GreedyPunterChun : public framework::SimplePunter {
 public:
  GreedyPunterChun();
  ~GreedyPunterChun() override;

  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  framework::GameMove Run() override;
  framework::GameMove Run(const std::vector<framework::GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

 private:
  gameutil::GameMapForAI themap;
};

} // namespace punter

#endif  // PUNTER_GREEDY_PUNTER_CHUN_H_
