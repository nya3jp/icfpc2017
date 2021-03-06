#ifndef FRAMEWORK_PASS_PUNTER_H_
#define FRAMEWORK_PASS_PUNTER_H_

#include "base/macros.h"
#include "framework/game.h"

namespace framework {

class PassPunter : public Punter {
 public:
  PassPunter();
  ~PassPunter() override;

  void SetUp(const common::SetUpData& args) override;
  GameMove Run(const std::vector<GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

 private:
  int punter_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(PassPunter);
};

}  // namespace framework


#endif  // FRAMEWORK_PASS_PUNTER_H_
