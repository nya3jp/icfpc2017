#include "punter/pass_punter.h"

#include <time.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

DEFINE_int32(sleep_duration, 0, "Length to sleep just before returning pass "
             "in milliseconds.");

namespace framework {

PassPunter::PassPunter() = default;
PassPunter::~PassPunter() = default;

GameMove PassPunter::Run(const std::vector<GameMove>& moves) {
  if (FLAGS_sleep_duration) {
    struct timespec duration;
    duration.tv_sec = FLAGS_sleep_duration / 1000;
    duration.tv_nsec = (FLAGS_sleep_duration % 1000) * 1000;
    CHECK(nanosleep(&duration, nullptr));
  }
  return GameMove::Pass(punter_id_);
}

void PassPunter::SetUp(const common::SetUpData& args) {
  punter_id_ = args.punter_id;
}

void PassPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  CHECK(state->GetInteger("punter_id", &punter_id_));
}

std::unique_ptr<base::Value> PassPunter::GetState() {
  auto value = base::MakeUnique<base::DictionaryValue>();
  value->SetInteger("punter_id", punter_id_);
  return std::move(value);
}

}  // namespace framework
