#include "punter/switching_punter.h"

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "punter/punter_factory.h"

namespace punter {

SwitchingPunter::SwitchingPunter(SwitchingPunter::ChooserType f)
  : f_(f) {}
SwitchingPunter::~SwitchingPunter() = default;

void SwitchingPunter::SetUp(const common::SetUpData& args) {
  chosen_ = PunterByName(f_(args));
  chosen_->SetUp(args);
}

framework::GameMove SwitchingPunter::Run(
    const std::vector<framework::GameMove>& moves) {
  return chosen_->Run(moves);
}

void SwitchingPunter::SetState(std::unique_ptr<base::Value> state_in) {
  chosen_->SetState(std::move(state_in));
}

std::unique_ptr<base::Value> SwitchingPunter::GetState() {
  return chosen_->GetState();
}

}  // namespace punter
