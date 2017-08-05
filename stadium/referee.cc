#include "stadium/referee.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"

namespace stadium {

struct Referee::SiteState {
  int id;
  bool is_mine;

  SiteState(int id, bool is_mine) : id(id), is_mine(is_mine) {}
};

struct Referee::RiverKey {
  int source;
  int target;

  RiverKey(int source, int target)
      : source(std::min(source, target)),
        target(std::max(source, target)) {}
  friend bool operator<(const RiverKey& a, const RiverKey& b) {
    return std::tie(a.source, a.target) < std::tie(b.source, b.target);
  }
};

struct Referee::RiverState {
  int source;
  int target;
  int punter_id;

  RiverState(int source, int target)
      : source(std::min(source, target)),
        target(std::max(source, target)),
        punter_id(-1) {}
};

// static
Referee::MapState Referee::MapState::FromMap(const Map& map) {
  MapState map_state;
  for (const Site& site : map.sites) {
    auto result = map_state.sites.insert(
        std::make_pair(site.id, SiteState(site.id, site.is_mine)));
    CHECK(result.second) << "Duplicated site: " << site.id;
  }
  for (const River& river : map.rivers) {
    auto result = map_state.rivers.insert(
        std::make_pair(RiverKey(river.source, river.target),
                       RiverState(river.source, river.target)));
    CHECK(result.second) << "Duplicated river: "
                         << river.source << "-" << river.target;
  }
  return map_state;
}

Referee::Referee() = default;
Referee::~Referee() = default;

void Referee::Setup(const std::vector<std::string>& names, const Map* map) {
  names_ = names;
  for (int punter_id = 0; punter_id < names.size(); ++punter_id) {
    LOG(INFO) << "P" << punter_id << ": " << names[punter_id];
  }

  map_state_ = MapState::FromMap(*map);
}

Move Referee::HandleMove(int turn_id, int punter_id, const Move& move) {
  Move actual_move = move;

  if (actual_move.punter_id != punter_id) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << names_[punter_id] << "\" "
               << "returned a move with a wrong punter ID "
               << actual_move.punter_id
               << ". This should a bug in the punter.";
    actual_move.punter_id = punter_id;
  }

  if (actual_move.type == Move::Type::CLAIM) {
    auto iter = map_state_.rivers.find(RiverKey(move.source, move.target));
    if (iter == map_state_.rivers.end()) {
      LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                 << ": Punter \"" << names_[punter_id] << "\" "
                 << "tried to claim a non-existence river "
                 << move.source << "-" << move.target
                 << ". Forcing to PASS.";
      actual_move = Move::MakePass(punter_id);
    } else {
      RiverState& river = iter->second;
      if (river.punter_id >= 0) {
        LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                   << ": Punter \"" << names_[punter_id] << "\" "
                   << "tried to claim a non-existence river "
                   << move.source << "-" << move.target
                   << ". Forcing to PASS.";
        actual_move = Move::MakePass(punter_id);
      } else {
        river.punter_id = punter_id;
      }
    }
  }

  if (actual_move.type == Move::Type::PASS) {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": PASS";
  } else {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": CLAIM " << move.source << "-" << move.target;
  }
  return actual_move;
}

void Referee::Finish() {
  LOG(INFO) << "Game finished.";
}

}  // namespace stadium
