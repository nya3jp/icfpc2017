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

void Master::RunGame(Map map, const common::Settings& settings) {
  CHECK(!punters_.empty());
  Initialize(std::move(map), settings);
  DoRunGame();
}

void Master::Initialize(Map map, const common::Settings& settings) {
  common::SetUpData args;
  args.num_punters = punters_.size();
  args.game_map = std::move(map);
  args.settings = settings;
  std::vector<PunterInfo> punter_info_list;
  for (int punter_id = 0; punter_id < punters_.size(); ++punter_id) {
    args.punter_id = punter_id;
    punter_info_list.push_back(punters_[punter_id]->SetUp(args));
  }

  map_ = std::move(args.game_map);
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
