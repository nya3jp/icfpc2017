#ifndef PUNTER_PUNTER_FACTORY_H__
#define PUNTER_PUNTER_FACTORY_H__

#include <memory>
#include <string>
#include "framework/game.h"

namespace punter {
std::unique_ptr<framework::Punter> PunterByName(const std::string& name);

class SwitchingPunter : public framework::Punter {
public:
  using ChooserType = std::function<std::string(const common::SetUpData&)>;
  explicit SwitchingPunter(ChooserType f);
  ~SwitchingPunter() override;

  void SetUp(const common::SetUpData& args) override;
  framework::GameMove Run(const std::vector<framework::GameMove>& moves) override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

  static std::string BenkeiOrJam(const common::SetUpData& args);
 private:
  ChooserType f_;
  std::unique_ptr<framework::Punter> chosen_;

  DISALLOW_COPY_AND_ASSIGN(SwitchingPunter);
};

}  // namespace punter

#endif  // PUNTER_PUNTER_FACTORY_H__
