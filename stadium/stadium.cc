#include "stadium/master.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "stadium/game_data.h"
#include "stadium/local_punter.h"

DEFINE_string(map, "", "Path to a map JSON file.");
DEFINE_bool(futures, false, "Enable Futures feature.");
DEFINE_bool(splurges, false, "Enable Splurges feature.");
DEFINE_bool(options, false, "Enable Options feature.");

namespace stadium {
namespace {

std::unique_ptr<Punter> MakePunterFromCommandLine(const std::string& arg) {
  return base::MakeUnique<LocalPunter>(arg);
}

void Main(int argc, char** argv) {
  if (FLAGS_map.empty() || argc == 1) {
    LOG(INFO) << "Usage: " << argv[0] << " --map=<mapfile>"
              << " '<punter 1 command line>' '<punter 2 command line>' ...";
    return;
  }
  Map map = ReadMapFromFileOrDie(FLAGS_map);

  common::Settings settings;
  settings.futures = FLAGS_futures;
  settings.splurges = FLAGS_splurges;
  settings.options = FLAGS_options;
  std::unique_ptr<Master> master = base::MakeUnique<Master>();
  for (int i = 1; i < argc; ++i) {
    master->AddPunter(MakePunterFromCommandLine(argv[i]));
  }

  master->RunGame(std::move(map), settings);
}

}  // namespace
}  // namespace stadium

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  google::InstallFailureSignalHandler();

  stadium::Main(argc, argv);
  return 0;
}
