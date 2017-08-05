#include "stadium/stub_punter.h"

#include <algorithm>

namespace stadium {

StubPunter::StubPunter() = default;

StubPunter::~StubPunter() = default;

std::string StubPunter::Setup(int punter_id,
                              int num_punters,
                              const Map* map) {
  punter_id_ = punter_id;

  for (const auto& river : map->rivers) {
    unclaimed_rivers_.insert(std::make_pair(
        std::min(river.source, river.target),
        std::max(river.source, river.target)));
  }

  return "STUB";
}

Move StubPunter::OnTurn(const std::vector<Move>& moves) {
  for (const auto& move : moves) {
    if (move.type == Move::Type::CLAIM) {
      unclaimed_rivers_.erase(std::make_pair(
          std::min(move.source, move.target),
          std::max(move.source, move.target)));
    }
  }
  auto iter = unclaimed_rivers_.begin();
  Move move = Move::MakeClaim(punter_id_, iter->first, iter->second);
  unclaimed_rivers_.erase(iter);
  return move;
}

}  // namespace stadium
