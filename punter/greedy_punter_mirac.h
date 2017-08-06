#ifndef PUNTER_GREEDY_PUNTER_MIRAC_H_
#define PUNTER_GREEDY_PUNTER_MIRAC_H_

#include "framework/simple_punter.h"

namespace punter {

class GreedyPunterMirac : public framework::SimplePunter {
 public:
  GreedyPunterMirac();
  ~GreedyPunterMirac() override;

  framework::GameMove Run() override;
};

} // namespace punter

#endif  // PUNTER_GREEDY_PUNTER_MIRAC_H_
