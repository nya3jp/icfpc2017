#include <fcntl.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "framework/game.h"
#include "gflags/gflags.h"
#include "punter/punter_factory.h"

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

  std::unique_ptr<Punter> punter = PunterByName(FLAGS_punter);
  Game game(std::move(punter));
  game.Run();
}
