#ifndef PUNTER_EXTENSION_EXAMPLE_PUNTER_H_
#define PUNTER_EXTENSION_EXAMPLE_PUNTER_H_

#include <random>

#include "framework/simple_punter.h"

namespace punter {

class ExtensionExamplePunter : public framework::SimplePunter {
 public:
  ExtensionExamplePunter();
  ~ExtensionExamplePunter() override;

  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  framework::GameMove Run() override;

 private:
};

} // namespace punter

#endif  // PUNTER_EXTENSION_EXAMPLE_PUNTER_H_
