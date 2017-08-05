#include "stadium/stub_punter.h"

namespace stadium {

StubPunter::StubPunter() = default;

StubPunter::~StubPunter() = default;

std::string StubPunter::Setup(int punter_id,
                               int num_punters,
                               const Map* map) {
  punter_id_ = punter_id;
  return "STUB";
}

Move StubPunter::OnTurn(const std::vector<Move>& moves) {
  return Move::MakePass(punter_id_);
}

}  // namespace stadium
