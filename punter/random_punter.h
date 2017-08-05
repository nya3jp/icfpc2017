#ifndef PUNTER_RANDOM_PUNTER_H_
#define PUNTER_RANDOM_PUNTER_H_

#include <random>

#include "framework/simple_punter.h"

namespace punter {

class RandomPunter : public framework::SimplePunter {
 public:
  RandomPunter();
  ~RandomPunter() override;

  framework::GameMove Run() override;

 private:
  std::default_random_engine rand_;
};

} // namespace punter

#endif  // PUNTER_RANDOM_PUNTER_H_
