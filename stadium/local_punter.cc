#include "stadium/local_punter.h"

namespace stadium {

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {}

LocalPunter::~LocalPunter() = default;

std::string LocalPunter::Setup(int punter_id,
                               int num_punters,
                               const Map* map) {
  punter_id_ = punter_id;
  LOG(ERROR) << "NOT IMPLEMENTED";
  return "not_implemented";
}

Move LocalPunter::OnTurn(const std::vector<Move>& moves) {
  LOG(ERROR) << "NOT IMPLEMENTED";
  return Move::MakePass(punter_id_);
}

}  // namespace stadium
