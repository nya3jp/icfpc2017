#include "base/memory/ptr_util.h"

#include "gflags/gflags.h"
#include "base/logging.h"

#include "framework/game.h"
#include "punter/random_punter.h"

using namespace framework;
using namespace punter;

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  Game game(base::MakeUnique<RandomPunter>());
  game.Run();
}
