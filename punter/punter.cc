#include <fcntl.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "framework/game.h"
#include "gflags/gflags.h"
#include "punter/benkei.h"
#include "punter/extension_example_punter.h"
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

using namespace framework;
using namespace punter;

DEFINE_string(punter, "RandomPunter", "Punter class.");

namespace {

void SetBlocking(int fd) {
  int flg = fcntl(fd, F_GETFL);
  CHECK_GE(flg, 0);
  CHECK_EQ(0, fcntl(fd, F_SETFL, flg & ~O_NONBLOCK));
}

}  // namespace

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  DLOG(INFO) << "Set blocking";
  SetBlocking(0);
  SetBlocking(1);
  SetBlocking(2);

  std::unique_ptr<Punter> punter;
  if (FLAGS_punter == "RandomPunter")
    punter = base::MakeUnique<RandomPunter>();
  else if (FLAGS_punter == "PassPunter")
    punter = base::MakeUnique<PassPunter>();
  else if (FLAGS_punter == "QuickPunter")
    punter = base::MakeUnique<QuickPunter>();
  else if (FLAGS_punter == "GreedyPunter")
    punter = base::MakeUnique<GreedyPunter>();
  else if (FLAGS_punter == "GreedyPunterChun")
    punter = base::MakeUnique<GreedyPunterChun>();
  else if (FLAGS_punter == "ExtensionExamplePunter")
    punter = base::MakeUnique<ExtensionExamplePunter>();
  else if (FLAGS_punter == "GreedyToJam")
    punter = base::MakeUnique<GreedyToJam>();
  else if (FLAGS_punter == "GreedyPunterMirac")
    punter = base::MakeUnique<GreedyPunterMirac>();
  else if (FLAGS_punter == "LazyPunter")
    punter = base::MakeUnique<LazyPunter>();
  else if (FLAGS_punter == "Benkei")
    punter = base::MakeUnique<Benkei>();
  else if (FLAGS_punter == "SimulatingPunter")
    punter = base::MakeUnique<SimulatingPunter>();
  else if (FLAGS_punter == "MetaPunter")
    punter = base::MakeUnique<MetaPunter>();
  else
    LOG(FATAL) << "invalid punter name: " << FLAGS_punter;
  Game game(std::move(punter));
  game.Run();
}
