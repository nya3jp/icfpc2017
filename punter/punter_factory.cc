#include "punter_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "punter/benkei.h"
#include "punter/extension_example_punter.h"
#include "punter/friendly_punter.h"
#include "punter/greedy_punter.h"
#include "punter/greedy_punter_chun.h"
#include "punter/greedy_punter_mirac.h"
#include "punter/greedy_to_jam.h"
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
  else
    LOG(FATAL) << "invalid punter name: " << name;
  return nullptr;
}

}  // namespace punter

