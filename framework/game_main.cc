#include "base/memory/ptr_util.h"

#include "gflags/gflags.h"
#include "base/logging.h"

#include "framework/game.h"
#include "framework/pass_punter.h"
#include "punter/extension_example_punter.h"
#include "punter/random_punter.h"
#include "punter/greedy_punter.h"
#include "punter/greedy_punter_chun.h"

using namespace framework;
using namespace punter;

DEFINE_string(punter, "RandomPunter", "Punter class.");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  std::unique_ptr<Punter> punter;
  if (FLAGS_punter == "RandomPunter")
    punter = base::MakeUnique<RandomPunter>();
  else if (FLAGS_punter == "PassPunter")
    punter = base::MakeUnique<PassPunter>();
  else if (FLAGS_punter == "GreedyPunter")
    punter = base::MakeUnique<GreedyPunter>();
  else if (FLAGS_punter == "GreedyPunterChun")
    punter = base::MakeUnique<GreedyPunterChun>();
  else if (FLAGS_punter == "ExtensionExamplePunter")
    punter = base::MakeUnique<ExtensionExamplePunter>();
  else
    LOG(FATAL) << "invalid punter name";
  Game game(std::move(punter));
  game.Run();
}
