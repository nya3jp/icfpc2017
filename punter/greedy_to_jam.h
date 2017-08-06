#ifndef PUNTER_GREEDY_TO_JAM_H_
#define PUNTER_GREEDY_TO_JAM_H_

#include "framework/simple_punter.h"
#include "punter/gamemapforai.h"

namespace punter {


class GreedyToJam : public framework::SimplePunter {
 public:
  GreedyToJam();
  ~GreedyToJam() override;

  void SetUp(const common::SetUpData& args) override;
  framework::GameMove Run() override;
  framework::GameMove Run(const std::vector<framework::GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

  struct movecandidate {
    int score;
    int edgeix;
    int sourceix;
    int targetix;
  };

 private:
  std::vector<movecandidate> list_candidates(int color);

  gameutil::GameMapForAI themap;
};

} // namespace punter

#endif  // PUNTER_GREEDY_TO_JAM_H_
