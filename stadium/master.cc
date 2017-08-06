#include "stadium/master.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace stadium {

Master::Master() = default;

Master::~Master() = default;

void Master::AddPunter(std::unique_ptr<Punter> punter) {
  punters_.emplace_back(std::move(punter));
}

void Master::RunGame(Map map, const Settings& settings) {
  CHECK(!punters_.empty());
  Initialize(std::move(map), settings);
  DoRunGame();
}

void Master::Initialize(Map map, const Settings& settings) {
  map_ = std::move(map);

  std::vector<PunterInfo> punter_info_list;
  for (int punter_id = 0; punter_id < punters_.size(); ++punter_id) {
    PunterInfo punter_info = punters_[punter_id]->Setup(
        punter_id, punters_.size(), &map_, settings);
    punter_info_list.push_back(std::move(punter_info));
  }

  referee_ = base::MakeUnique<Referee>();
  referee_->Setup(punter_info_list, &map_);

  last_moves_.clear();
  for (int punter_id = 0; punter_id < punters_.size(); ++punter_id) {
    last_moves_.emplace_back(Move::Pass(punter_id));
  }
}

void Master::DoRunGame() {
  for (int turn_id = 0; turn_id < map_.rivers.size(); ++turn_id) {
    int punter_id = turn_id % punters_.size();
    Move move = punters_[punter_id]->OnTurn(last_moves_);
    Move actual_move = referee_->HandleMove(turn_id, punter_id, move);
    CHECK_EQ(actual_move.punter_id, punter_id);
    last_moves_[punter_id] = actual_move;
  }
  referee_->Finish();
}

}  // namespace stadium
