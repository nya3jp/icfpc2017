#include "punter_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "punter/benkei.h"
#include "punter/extension_example_punter.h"
#include "punter/friendly_punter.h"
#include "punter/friendly_punter2.h"
#include "punter/future_punter.h"
#include "punter/greedy_punter.h"
#include "punter/greedy_punter_chun.h"
#include "punter/greedy_punter_mirac.h"
#include "punter/greedy_to_jam.h"
#include "punter/jammer.h"
#include "punter/lazy_punter.h"
#include "punter/meta_punter.h"
#include "punter/pass_punter.h"
#include "punter/quick_punter.h"
#include "punter/random_punter.h"
#include "punter/simulating_punter.h"

namespace punter {

std::unique_ptr<framework::Punter> PunterByName(const std::string& name) {
  if (name == "RandomPunter")
    return base::MakeUnique<RandomPunter>();
  else if (name == "PassPunter")
    return base::MakeUnique<framework::PassPunter>();
  else if (name == "QuickPunter")
    return base::MakeUnique<QuickPunter>();
  else if (name == "GreedyPunter")
    return base::MakeUnique<GreedyPunter>();
  else if (name == "GreedyPunterChun")
    return base::MakeUnique<GreedyPunterChun>();
  else if (name == "ExtensionExamplePunter")
    return base::MakeUnique<ExtensionExamplePunter>();
  else if (name == "GreedyToJam")
    return base::MakeUnique<GreedyToJam>();
  else if (name == "Jammer")
    return base::MakeUnique<Jammer>();
  else if (name == "GreedyPunterMirac")
    return base::MakeUnique<GreedyPunterMirac>();
  else if (name == "LazyPunter")
    return base::MakeUnique<LazyPunter>();
  else if (name == "Benkei")
    return base::MakeUnique<Benkei>();
  else if (name == "SimulatingPunter")
    return base::MakeUnique<SimulatingPunter>();
  else if (name == "MetaPunter")
    return base::MakeUnique<MetaPunter>();
  else if (name == "FriendlyPunter")
    return base::MakeUnique<FriendlyPunter>();
  else if (name == "FriendlyPunter2")
    return base::MakeUnique<FriendlyPunter2>();
  else if (name == "BenkeiOrJam")
    return base::MakeUnique<SwitchingPunter>(SwitchingPunter::BenkeiOrJam);
  else if (name == "FuturePunter")
    return base::MakeUnique<FuturePunter>();
  else if (name == "MiracOrJamOrFriendly")
    return base::MakeUnique<SwitchingPunter>(SwitchingPunter::MiracOrJamOrFriendly);
  else if (name == "MiracOrJamOrFriendlyOrFuture")
    return base::MakeUnique<SwitchingPunter>(SwitchingPunter::MiracOrJamOrFriendlyOrFuture);
  else
    LOG(FATAL) << "invalid punter name: " << name;
  return nullptr;
}

// SwitchingPunter.
SwitchingPunter::SwitchingPunter(SwitchingPunter::ChooserType f)
  : f_(f) {}
SwitchingPunter::~SwitchingPunter() = default;

void SwitchingPunter::SetUp(const common::SetUpData& args) {
  name_ = f_(args);
  core_ = PunterByName(name_);
  core_->SetUp(args);
}

std::vector<common::Future> SwitchingPunter::GetFutures() {
  return core_->GetFutures();
}

framework::GameMove SwitchingPunter::Run(
    const std::vector<framework::GameMove>& moves) {
  return core_->Run(moves);
}

void SwitchingPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  CHECK(state->GetString("core", &name_));
  core_ = PunterByName(name_);
  core_->SetState(std::move(state));
}

std::unique_ptr<base::Value> SwitchingPunter::GetState() {
  auto value = base::DictionaryValue::From(core_->GetState());
  value->SetString("core", name_);
  return value;
}

// static
std::string SwitchingPunter::BenkeiOrJam(const common::SetUpData& args) {
  if (args.num_punters == 2 &&
      args.game_map.sites.size() <  100) { return "Jammer"; }
  return "Benkei";
}

// static
std::string SwitchingPunter::MiracOrJamOrFriendly(const common::SetUpData& args) {
  if (args.game_map.sites.size() >= 800 &&
      args.game_map.mines.size() >= 8) { return "FriendlyPunter"; }
  if (args.num_punters == 2 &&
      args.game_map.sites.size() <  100) { return "Jammer"; }
  return "GreedyPunterMirac";
}

// static
std::string SwitchingPunter::MiracOrJamOrFriendlyOrFuture(const common::SetUpData& args) {
  if (args.game_map.sites.size() >= 800 &&
      args.game_map.mines.size() >= 8) { return "FriendlyPunter"; }
  if (args.num_punters == 2 &&
      args.game_map.sites.size() < 100) {
    if (args.settings.futures) {
      return "FuturePunter";
    }
    return "Jammer";
  }
  return "GreedyPunterMirac";
}

}  // namespace punter
