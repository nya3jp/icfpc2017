#include "stadium/master.h"

#include <utility>

#include "base/logging.h"

namespace stadium {

Master::Master() = default;

Master::~Master() = default;

void Master::AddPunter(std::unique_ptr<Punter> punter) {
  punters_.emplace_back(std::move(punter));
}

void Master::RunGame(const Map& map) {
  CHECK(!punters.empty());
  Initialize(map);
  DoRunGame();
}

void Master::Initialize(const Map& map) {
  referee_ = base::MakeUnique<Referee>();
  referee->Initialize(punters_.size(), map);

  for (int punter_id = 0; punter_id < punters_.size(); ++punter_id) {
    punters_[punter_id]->Initialize(punter_id, punters_.size(), map);
  }

  last_moves_.clear();
  for (int punter_id = 0; punter_id < punters_.size(); ++punter_id) {
    last_moves_.emplace_back(Move::MakePass(punter_id));
  }
}

void Master::DoRunGame() {
  for (int turn_id = 0; turn_id < map_.rivers.size(); ++turn_id) {
    int punter_id = turn_id % map_.revers.size();
    Move move = punters_[punter_id]->OnTurn(last_moves_);
    CHECK_EQ(move.punter_id, punter_id);
    referee_->OnMove(move);
    last_moves_[punter_id] = move;
  }
  referee_->Finish();
}

}  // namespace stadium
