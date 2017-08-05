#include "stadium/referee.h"

#include "base/logging.h"

namespace stadium {

Referee::Referee() = default;
Referee::~Referee() = default;

void Referee::Setup(const std::vector<std::string>& names, const Map* map) {
  names_ = names;
}

Move Referee::HandleMove(const Move& move, int punter_id) {
  LOG(ERROR) << "NOT IMPLEMENTED";
  return move;
}

void Referee::Finish() {
  LOG(ERROR) << "NOT IMPLEMENTED";
}

}  // namespace stadium
