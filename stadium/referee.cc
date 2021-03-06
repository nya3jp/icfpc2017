#include "stadium/referee.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "common/scorer.h"
#include "gflags/gflags.h"

DEFINE_string(result_json, "", "Path to output result json");

namespace stadium {

namespace {

std::string PrintVectorInt(const std::vector<int>& v) {
  std::stringstream ss;
  ss << v.front();
  for (int i = 1; i < v.size(); ++i)
    ss << ", " << v[i];
  return std::move(ss.str());
}

void WriteResults(const std::string& path,
                  const std::vector<int>& scores,
                  const std::vector<Move>& moves) {
  auto json = base::MakeUnique<base::DictionaryValue>();

  auto scores_value = base::MakeUnique<base::ListValue>();
  scores_value->Reserve(scores.size());
  for (int score : scores) {
    scores_value->AppendInteger(score);
  }
  json->Set("scores", std::move(scores_value));

  auto moves_value = base::MakeUnique<base::ListValue>();
  moves_value->Reserve(moves.size());
  for (const auto& move : moves) {
    auto move_value = base::MakeUnique<base::DictionaryValue>();
    switch (move.type) {
      case Move::Type::CLAIM: {
        auto claim_value = base::MakeUnique<base::DictionaryValue>();
        claim_value->SetInteger("punter", move.punter_id);
        claim_value->SetInteger("source", move.source);
        claim_value->SetInteger("target", move.target);
        move_value->Set("claim", std::move(claim_value));
        break;
      }
      case Move::Type::PASS: {
        auto pass_value = base::MakeUnique<base::DictionaryValue>();
        pass_value->SetInteger("punter", move.punter_id);
        move_value->Set("pass", std::move(pass_value));
        break;
      }
      case Move::Type::SPLURGE: {
        auto splurge_value = base::MakeUnique<base::DictionaryValue>();
        splurge_value->SetInteger("punter", move.punter_id);
        auto route_value = base::MakeUnique<base::ListValue>();
        route_value->Reserve(move.route.size());
        for (const auto& site : move.route)
          route_value->Append(base::MakeUnique<base::Value>(site));
        splurge_value->Set("route", std::move(route_value));
        move_value->Set("splurge", std::move(splurge_value));
        break;
      }
      case Move::Type::OPTION: {
        auto option_value = base::MakeUnique<base::DictionaryValue>();
        option_value->SetInteger("punter", move.punter_id);
        option_value->SetInteger("source", move.source);
        option_value->SetInteger("target", move.target);
        move_value->Set("option", std::move(option_value));
        break;
      }
    }
    moves_value->Append(std::move(move_value));
  }
  json->Set("moves", std::move(moves_value));

  std::string output;
  CHECK(base::JSONWriter::Write(*json, &output));
  base::WriteFile(base::FilePath(path), output.data(), output.size());
}

}

struct Referee::SiteState {
  int id;
  // bool is_mine;
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
  int option_punter_id;

  RiverState(int source, int target)
      : source(std::min(source, target)),
        target(std::max(source, target)),
        punter_id(-1),
        option_punter_id(-1) {}
};

// static
Referee::MapState Referee::MapState::FromMap(const Map& map) {
  MapState map_state;
  for (const Site& site : map.sites) {
    auto result = map_state.sites.insert(
        std::make_pair(site.id, SiteState{site.id}));
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

void Referee::Setup(const std::vector<PunterInfo>& punter_info_list,
                    const Map* map) {
  punter_info_list_ = punter_info_list;
  for (int punter_id = 0; punter_id < punter_info_list.size(); ++punter_id) {
    LOG(INFO) << "P" << punter_id << ": " << punter_info_list[punter_id].name;
  }

  pass_count_.resize(punter_info_list.size());
  options_remaining_.resize(punter_info_list.size(), map->mines.size());

  map_state_ = MapState::FromMap(*map);
  common::Scorer scorer(&scorer_);
  scorer.Initialize(punter_info_list.size(), *map);
  for (size_t punter_id = 0; punter_id < punter_info_list.size();
       ++punter_id) {
    scorer.AddFuture(punter_id, punter_info_list[punter_id].futures);
  }
}

bool Referee::ValidateClaim(const Move& move, int turn_id, int punter_id) {
  auto iter = map_state_.rivers.find(RiverKey(move.source, move.target));
  if (iter == map_state_.rivers.end()) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "tried to claim a non-existence river "
               << move.source << "-" << move.target
               << ". Forcing to PASS.";
    return false;
  }

  RiverState& river = iter->second;
  if (river.punter_id >= 0) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name
               << "\" tried to claim a already-used river "
               << move.source << "-" << move.target
               << ". Forcing to PASS.";
    return false;
  }

  river.punter_id = punter_id;

  return true;
}

bool Referee::ValidateSplurge(const Move& move, int turn_id, int punter_id) {
  if (move.route.size() - 1 > pass_count_[punter_id] + 1) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "tried to splurge after insufficient number of passes ("
               << move.route.size() - 1 << " <= "
               << (pass_count_[punter_id] + 1)
               << "). Forcing to PASS.";
    return false;
  }

  if (move.route.size() < 2) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "tried to splurge with less than 2 sites "
               << PrintVectorInt(move.route)
               << ". Forcing to PASS.";
    return false;
  }

  int options_remaining = options_remaining_[punter_id];

  for (int i = 0; i + 1 < move.route.size(); ++i) {
    int s = move.route[i];
    int t = move.route[i + 1];
    auto iter = map_state_.rivers.find(RiverKey(s, t));
    if (iter == map_state_.rivers.end()) {
      LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                 << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
                 << "tried to splurge over a non-existence river "
                 << s << "-" << t
                 << ". Forcing to PASS.";
      return false;
    }

    const RiverState& river = iter->second;
    if (river.punter_id >= 0) {
      if (river.option_punter_id >= 0) {
        LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                   << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
                   << "tried to splurge over a already-used (option) river "
                   << s << "-" << t
                   << ". Forcing to PASS.";
        return false;
      }

      if (options_remaining <= 0) {
        LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                   << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
                   << "tried to splurge over a river with optioning with no remaining options"
                   << s << "-" << t
                   << ". Forcing to PASS.";
        return false;
      }

      --options_remaining;
    }
  }

  for (int i = 0; i + 1 < move.route.size(); ++i) {
    int s = move.route[i];
    int t = move.route[i + 1];
    auto iter = map_state_.rivers.find(RiverKey(s, t));
    RiverState& river = iter->second;
    if (river.punter_id == -1) {
      river.punter_id = punter_id;
    } else {
      river.option_punter_id = punter_id;
      --options_remaining_[punter_id];
    }
  }

  return true;
}

bool Referee::ValidateOption(const Move& move, int turn_id, int punter_id) {
  if (options_remaining_[punter_id] <= 0) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "tried to option with no remaining options"
               << ". Forcing to PASS.";
    return false;
  }

  auto iter = map_state_.rivers.find(RiverKey(move.source, move.target));
  if (iter == map_state_.rivers.end()) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "tried to option a non-existence river "
               << move.source << "-" << move.target
               << ". Forcing to PASS.";
    return false;
  }

  RiverState& river = iter->second;
  if (river.punter_id == -1) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name
               << "\" tried to option a not-yet-used river "
               << move.source << "-" << move.target
               << ". Forcing to PASS.";
    return false;
  }

  if (river.option_punter_id >= 0) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name
               << "\" tried to option a already-used (option) river "
               << move.source << "-" << move.target
               << ". Forcing to PASS.";
    return false;
  }

  river.option_punter_id = punter_id;
  --options_remaining_[punter_id];

  return true;
}

Move Referee::HandleMove(int turn_id, int punter_id, const Move& move) {
  Move actual_move = move;

  if (actual_move.punter_id != punter_id) {
    LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
               << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
               << "returned a move with a wrong punter ID "
               << actual_move.punter_id
               << ". This should a bug in the punter.";
    actual_move.punter_id = punter_id;
  }

  switch (actual_move.type) {
    case Move::Type::CLAIM:
      if (!ValidateClaim(move, turn_id, punter_id))
        actual_move = Move::Pass(punter_id);
      break;
    case Move::Type::SPLURGE:
      if (!ValidateSplurge(move, turn_id, punter_id))
        actual_move = Move::Pass(punter_id);
      break;
    case Move::Type::OPTION:
      if (!ValidateOption(move, turn_id, punter_id))
        actual_move = Move::Pass(punter_id);
      break;
    case Move::Type::PASS:
      break;
  }

  if (actual_move.type == Move::Type::PASS) {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": PASS";
    ++pass_count_[punter_id];
  } else if (actual_move.type == Move::Type::CLAIM) {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": CLAIM " << move.source << "-" << move.target;
    common::Scorer(&scorer_).Claim(punter_id, move.source, move.target);
    pass_count_[punter_id] = 0;
  } else if (actual_move.type == Move::Type::SPLURGE) {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": SPLURGE " << PrintVectorInt(move.route);
    common::Scorer(&scorer_).Splurge(punter_id, move.route);
    pass_count_[punter_id] = 0;
  } else {
    LOG(INFO) << "LOG: [" << turn_id << "] P" << punter_id
              << ": OPTION " << move.source << "-" << move.target;
    common::Scorer(&scorer_).Option(punter_id, move.source, move.target);
    pass_count_[punter_id] = 0;
  }

  ComputeScores();

  move_history_.push_back(actual_move);
  return actual_move;
}

std::vector<int> Referee::Finish() {
  LOG(INFO) << "Game finished.";
  std::vector<int> scores = ComputeScores();
  if (!FLAGS_result_json.empty())
    WriteResults(FLAGS_result_json, scores, move_history_);
  return scores;
}

std::vector<int> Referee::ComputeScores() const {
  common::Scorer scorer(&scorer_);
  std::vector<int> scores;
  for (size_t punter_id = 0; punter_id < punter_info_list_.size();
       ++punter_id) {
    scores.push_back(scorer.GetScore(punter_id));
    LOG(INFO) << "Punter: " << punter_id << ", Score: " << scores.back();
  }
  return scores;
}

}  // namespace stadium
