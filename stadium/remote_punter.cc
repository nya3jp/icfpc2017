#include "stadium/remote_punter.h"

namespace stadium {

RemotePunter::RemotePunter(int port) : port_(port) {}

RemotePunter::~RemotePunter() = default;

PunterInfo RemotePunter::Setup(int punter_id,
                               int num_punters,
                               const Map* map,
                               const Settings& settings) {
  punter_id_ = punter_id;
  LOG(ERROR) << "NOT IMPLEMENTED";
  return {"not_implemented", {}};
}

Move RemotePunter::OnTurn(const std::vector<Move>& moves) {
  LOG(ERROR) << "NOT IMPLEMENTED";
  return Move::Pass(punter_id_);
}

}  // namespace stadium
