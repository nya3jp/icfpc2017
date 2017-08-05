#include "punter/extension_example_punter.h"

#include "framework/game.h"
#include "punter/extension_example.pb.h"

namespace punter {

ExtensionExamplePunter::ExtensionExamplePunter() = default;
ExtensionExamplePunter::~ExtensionExamplePunter() = default;

void ExtensionExamplePunter::SetUp(
    int punter_id, int num_punters, const framework::GameMap& game_map) {
  proto_.MutableExtension(ExtensionExample::example_ext)->set_count(1);
}

framework::GameMove ExtensionExamplePunter::Run() {
  int now = proto_.GetExtension(ExtensionExample::example_ext).count();
  LOG(INFO) << "Count: " << now;
  proto_.MutableExtension(ExtensionExample::example_ext)->set_count(now + 1);
  return framework::GameMove::Pass(punter_id_);
}

} // namespace punter
