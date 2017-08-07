#ifndef PUNTER_SWITCHING_PUNTER_H__
#define PUNTER_SWITCHING_PUNTER_H__

#include <memory>
#include <string>

#include "base/macros.h"
#include "framework/game.h"

namespace punter {

class SwitchingPunter : public framework::Punter {
public:
  using ChooserType = std::function<std::string(const common::SetUpData&)>;
  explicit SwitchingPunter(ChooserType f);
  ~SwitchingPunter() override;

  void SetUp(const common::SetUpData& args) override;
  framework::GameMove Run(const std::vector<framework::GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

  virtual std::string ChoosePunter(const common::SetUpData& args) = 0;
  
 private:
  ChooserType f_;
  std::unique_ptr<framework::Punter> chosen_;

  DISALLOW_COPY_AND_ASSIGN(SwitchingPunter);
};

}  // namespace punter

#endif  // PUNTER_SWITCHING_PUNTER_H__
