#ifndef PUNTER_PUNTER_FACTORY_H__
#define PUNTER_PUNTER_FACTORY_H__

#include <memory>
#include <string>
#include "framework/game.h"

namespace punter {
// The factory function to create Punter entity from the name.
std::unique_ptr<framework::Punter> PunterByName(const std::string& name);

// Meta punter to switch punters based on the game state.
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
  static std::string MiracOrJamOrFriendly(const common::SetUpData& args);

 private:
  ChooserType f_;
  std::string name_;
  std::unique_ptr<framework::Punter> core_;

  DISALLOW_COPY_AND_ASSIGN(SwitchingPunter);
};

}  // namespace punter

#endif  // PUNTER_PUNTER_FACTORY_H__
