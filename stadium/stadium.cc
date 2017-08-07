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
#include "stadium/remote_punter.h"
#include "stadium/stub_punter.h"

DEFINE_string(map, "", "Path to a map JSON file.");
DEFINE_bool(futures, false, "Enable Futures feature.");
DEFINE_bool(splurges, false, "Enable Splurges feature.");
DEFINE_bool(options, false, "Enable Options feature.");

namespace stadium {
namespace {

std::unique_ptr<Punter> MakePunterFromCommandLine(const std::string& arg) {
  if (arg == "stub") {
    return base::MakeUnique<StubPunter>();
  }
  if (base::StartsWith(arg, ":", base::CompareCase::SENSITIVE)) {
    int port;
    CHECK(base::StringToInt(arg.substr(1), &port));
    return base::MakeUnique<RemotePunter>(port);
  }
  return base::MakeUnique<LocalPunter>(arg);
}

void Main(int argc, char** argv) {
  Map map = ReadMapFromFileOrDie(FLAGS_map);

  if (argc == 1) {
    LOG(FATAL) << "No punter argument specified!";
  }

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
  google::InstallFailureSignalHandler();

  stadium::Main(argc, argv);
  return 0;
}
