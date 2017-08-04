#include "stadium/master.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "stadium/game_states.h"

DEFINE_string(map, "", "Path to a map JSON file.");

namespace stadium {
namespace {

void Main(int argc, char** argv) {
  Map map = Map::ReadFromFileOrDie(FLAGS_map);

  if (argc == 1) {
    LOG(FATAL) << "No argument specified!";
  }

  std::unique_ptr<Master> master = base::MakeUnique<Master>();
  for (int i = 1; i < argc; ++i) {
    master->AddPunter(MakePunterFromCommandLine(argv[i]));
  }

  master->RunGame(map);
}

}  // namespace
}  // namespace stadium

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  stadium::Main(argc, argv);
  return 0;
}
