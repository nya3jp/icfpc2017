#include "stadium/remote_punter.h"

namespace stadium {

RemotePunter::RemotePunter(int port) : port_(port) {}

RemotePunter::~RemotePunter() = default;

PunterInfo RemotePunter::SetUp(const common::SetUpData& args) {
  punter_id_ = args.punter_id;
  LOG(ERROR) << "NOT IMPLEMENTED";
  return {"not_implemented", {}};
}

base::Optional<Move> RemotePunter::OnTurn(const std::vector<Move>& moves) {
  LOG(ERROR) << "NOT IMPLEMENTED";
  return Move::Pass(punter_id_);
}

}  // namespace stadium
