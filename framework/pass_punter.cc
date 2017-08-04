#include "framework/pass_punter.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace framework {

PassPunter::PassPunter() = default;
PassPunter::~PassPunter() = default;

GameMove PassPunter::Run(const std::vector<GameMove>& moves) {
  return {GameMove::Type::PASS, punter_id_};
}

void PassPunter::Initialize(
    int punter_id, int num_punters, const GameMap& game_map) {
  punter_id_ = punter_id;
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
