#include "punter/random_punter.h"

#include "base/memory/ptr_util.h"

namespace punter {

RandomPunter::RandomPunter()
  : rand_(std::random_device()()) {
}
RandomPunter::~RandomPunter() = default;

framework::GameMove RandomPunter::Run() {
  std::vector<framework::SimplePunter::RiverWithPunter*> candidates;
  for (auto& r : rivers_) {
    if (r.punter == -1) {
      candidates.push_back(&r);
    }
  }

  CHECK(candidates.size() > 0);
  std::uniform_int_distribution<> dist(0, candidates.size() - 1);

  auto river = candidates[dist(rand_)];
  return {framework::GameMove::Type::CLAIM, punter_id_, river->source, river->target};
}

void RandomPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  base::DictionaryValue* value;
  CHECK(state->GetDictionary("random", &value));
  // Deserialize punter-specific state here.

  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> RandomPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  auto value = base::MakeUnique<base::DictionaryValue>();
  // Serialize punter-specific state here.

  state->Set("random", std::move(value));
  return state;
}

} // namespace punter
