#include "punter/random_punter.h"

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

} // namespace punter
