#include "punter/extension_example_punter.h"

#include "framework/game.h"
#include "punter/extension_example.pb.h"

namespace punter {

ExtensionExamplePunter::ExtensionExamplePunter() = default;
ExtensionExamplePunter::~ExtensionExamplePunter() = default;

void ExtensionExamplePunter::SetUp(const common::SetUpData& args) {
  // Note: Don't store proto_.MutableExtension pointer into the private fields
  // here because the extensions is likely to be reallocated by SetState.
  proto_.MutableExtension(ExtensionExample::example_ext)->set_count(1);
}

framework::GameMove ExtensionExamplePunter::Run() {
  const int now = proto_.GetExtension(ExtensionExample::example_ext).count();
  // LOG(INFO) so that this is visible in opt mode.
  LOG(INFO) << "Count: " << now;
  proto_.MutableExtension(ExtensionExample::example_ext)->set_count(now + 1);
  return framework::GameMove::Pass(punter_id_);
}

} // namespace punter
