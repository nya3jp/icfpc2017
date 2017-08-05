#include "stadium/referee.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "gflags/gflags.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"

DEFINE_string(result_json, "", "Path to output result json");

namespace stadium {

namespace {

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

void Referee::Setup(const std::vector<PunterInfo>& punter_info_list,
                    const Map* map) {
  punter_info_list_ = punter_info_list;
  for (int punter_id = 0; punter_id < punter_info_list.size(); ++punter_id) {
    LOG(INFO) << "P" << punter_id << ": " << punter_info_list[punter_id].name;
  }

  map_state_ = MapState::FromMap(*map);
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

  if (actual_move.type == Move::Type::CLAIM) {
    auto iter = map_state_.rivers.find(RiverKey(move.source, move.target));
    if (iter == map_state_.rivers.end()) {
      LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                 << ": Punter \"" << punter_info_list_[punter_id].name << "\" "
                 << "tried to claim a non-existence river "
                 << move.source << "-" << move.target
                 << ". Forcing to PASS.";
      actual_move = Move::MakePass(punter_id);
    } else {
      RiverState& river = iter->second;
      if (river.punter_id >= 0) {
        LOG(ERROR) << "BUG: [" << turn_id << "] P" << punter_id
                   << ": Punter \"" << punter_info_list_[punter_id].name
                   << "\" tried to claim a already-used river "
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
  move_history_.push_back(actual_move);
  return actual_move;
}

void Referee::Finish() {
  LOG(INFO) << "Game finished.";
  std::vector<int> scores = ComputeScores();
  if (!FLAGS_result_json.empty())
    WriteResults(FLAGS_result_json, scores, move_history_);
}

std::vector<int> Referee::ComputeScores() const {
  std::multimap<int, const RiverState*> edges;
  for (const auto& entry : map_state_.rivers) {
    edges.emplace(entry.first.source, &entry.second);
    edges.emplace(entry.first.target, &entry.second);
  }

  std::vector<int> mines;
  for (const auto& entry : map_state_.sites) {
    if (entry.second.is_mine)
      mines.push_back(entry.second.id);
  }

  // For each mine, distance map from each node to the mine.
  std::vector<std::map<int, int>> dist_map_list(mines.size());
  for (int i = 0; i < mines.size(); ++i) {
    auto& dist_map = dist_map_list[i];
    std::queue<int> q;
    q.push(mines[i]);
    dist_map[mines[i]] = 0;
    while (!q.empty()) {
      int site_id = q.front();
      q.pop();
      DCHECK(dist_map.count(site_id));
      int dist = dist_map[site_id];
      for (auto range = edges.equal_range(site_id);
           range.first != range.second; ++range.first) {
        const RiverState* river_state = range.first->second;
        int next_site_id =
            river_state->source == site_id ?
            river_state->target : river_state->source;
        if (dist_map.count(next_site_id))
          continue;
        dist_map[next_site_id] = dist + 1;
        q.push(next_site_id);
      }
    }
  }

  std::vector<int> scores;
  for (int punter_id = 0; punter_id < punter_info_list_.size(); ++punter_id) {
    int score = 0;
    for (int j = 0; j < mines.size(); ++j) {
      int mine = mines[j];
      const auto& dist_map = dist_map_list[j];

      std::queue<int> q;
      q.push(mine);
      std::set<int> visited;

      // Traverse graph.
      while (!q.empty()) {
        int site_id = q.front();
        q.pop();

        for (auto range = edges.equal_range(site_id);
             range.first != range.second; ++range.first) {
          const RiverState* river_state = range.first->second;
          if (river_state->punter_id != punter_id)
            continue;
          int next_site_id =
              river_state->source == site_id ?
              river_state->target : river_state->source;
          if (!visited.insert(next_site_id).second)
            continue;

          // First visit. Accumulate the score.
          auto it = dist_map.find(next_site_id);
          DCHECK(it != dist_map.end());
          int dist = it->second;
          score += dist * dist;

          q.push(next_site_id);
        }
      }

      // Future bonus.
      for (const auto& future : punter_info_list_[punter_id].futures) {
        if (future.source != mine)
          continue;
        auto it = dist_map.find(future.target);
        CHECK(it != dist_map.end());  // TODO: FIX ME. ... spec?
        int dist = it->second;
        dist = dist * dist * dist;
        if (!visited.count(future.target)) {
          dist = -dist;
        }
        score += dist;
      }
    }
    scores.push_back(score);
    LOG(INFO) << "Punter: " << punter_id << ", Score: " << score;
  }

  return scores;
}

}  // namespace stadium
