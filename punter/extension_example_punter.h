#ifndef PUNTER_EXTENSION_EXAMPLE_PUNTER_H_
#define PUNTER_EXTENSION_EXAMPLE_PUNTER_H_

#include <random>

#include "framework/simple_punter.h"
#include "punter/extension_example.pb.h"

namespace punter {

// Extension Example Punter just passes with LOG(INFO) the current turn.
class ExtensionExamplePunter : public framework::SimplePunter {
 public:
  ExtensionExamplePunter();
  ~ExtensionExamplePunter() override;

  void SetUp(const common::SetUpData& args) override;
  framework::GameMove Run() override;

 private:
};

} // namespace punter

#endif  // PUNTER_EXTENSION_EXAMPLE_PUNTER_H_
