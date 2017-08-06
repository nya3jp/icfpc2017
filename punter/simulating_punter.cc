#include "punter/simulating_punter.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/ptr_util.h"
#include "punter/random_punter.h"

namespace punter {

using framework::GameMove;

struct Snapshot {
  std::vector<GameMove> moves;  // As key.
  std::vector<int> scores;
  std::string key;
  int total_score;
};

namespace {

std::string MovesToKey(std::vector<GameMove> moves) {
  std::sort(moves.begin(), moves.end(),
            [](const GameMove& lhs, const GameMove& rhs) {
              return lhs.punter_id != rhs.punter_id ?
                lhs.punter_id < rhs.punter_id :
                lhs.source != rhs.source ? lhs.source < rhs.source :
                lhs.target < rhs.target; });
  std::ostringstream oss;
  for (const auto& m : moves) {
    oss << m.punter_id << " " << m.source << " " << m.target;
  }
  return oss.str();
}

int TotalScore(int punter_id, const std::vector<int>& scores) {
  int rival_score = 0;
  for (size_t i = 0; i < scores.size(); ++i) {
    if (static_cast<int>(i) == punter_id) {continue;}
    rival_score = std::max(scores[i], rival_score);
  }
  return scores[punter_id] - rival_score;
}

Snapshot SnapshotFromMove(
    int punter_id,
    const std::vector<GameMove>& moves,
    std::vector<int>& scores) {
  Snapshot result;
  result.moves = moves;
  result.scores = scores;
  result.key = MovesToKey(moves);
  result.total_score = TotalScore(punter_id, scores);
  return result;
}

void UniqifyByMove(std::vector<Snapshot>* snapshot) {
  std::unordered_map<std::string, const Snapshot*> snapshot_map;
  for (const auto& s : *snapshot) {
    if (snapshot_map.count(s.key)) {
      continue;
    }
    snapshot_map[s.key] = &s;
  }
  std::vector<Snapshot> new_snapshots;
  new_snapshots.resize(snapshot_map.size());
  for (const auto& kv : snapshot_map) {
    new_snapshots.push_back(*kv.second);
  }
  snapshot->swap(new_snapshots);
}

GameMove ClaimRiver(int id, const framework::RiverProto& r) {
  int source = std::min(r.source(), r.target());
  int target = r.source() + r.target() - source;
  return GameMove::Claim(id, source, target);
}

}  // namespace

SimulatingPunter::SimulatingPunter() = default;
SimulatingPunter::~SimulatingPunter() = default;

void SimulatingPunter::SetUp(const common::SetUpData& args) {
  SimplePunter::SetUp(args);

  // TODO: fix this.
  punter_ = base::MakeUnique<RandomPunter>();
  punter_->SetUp(args);
}

framework::GameMove SimulatingPunter::Run() {
  old_moves_.clear();
  for (const auto& r : *rivers_) {
    if (r.punter() >= 0) {
      old_moves_.push_back(ClaimRiver(r.punter(), r));
    }
  }
  // TODO:
  // Feed last posts into punters_ when FeedMove is available.
  const int maxStep = 5;
  std::vector<Snapshot> old_snapshot{GenerateSnapshot({})};
  for (int step = 0; step < maxStep; ++step) {
    std::vector<Snapshot> new_state;
    for (const auto& state : old_snapshot) {
      GenerateNextSnapshots(state, &new_state);
      ShrinkToTop(&new_state);
      old_snapshot.swap(new_state);
    }
  }
  return old_snapshot[0].moves[0];
}

void SimulatingPunter::ScoreFromMoves(
    const std::vector<GameMove>& moves,
    std::vector<int> *scores) const {
  // Todo: call scorerer-san.
}

Snapshot SimulatingPunter::GenerateSnapshot(
    const std::vector<GameMove>& moves) const {
  std::vector<int> scores;
  ScoreFromMoves(moves, &scores);
  return SnapshotFromMove(punter_id_, moves, scores);
}

void SimulatingPunter::GenerateNextSnapshots(
    const Snapshot& old,
    std::vector<Snapshot>* new_snapshots) {
  std::vector<GameMove> cur_moves(old_moves_);
  copy(old.moves.begin(), old.moves.end(),
       std::back_inserter(cur_moves));
  for (const auto& r : *rivers_) {
    if (r.punter() >= 0)
      continue;
    std::vector<GameMove> new_moves{ClaimRiver(punter_id_, r)};
    for (int i = 1; i < num_punters_; ++i) {
      // int cur_punter = (punter_id_ + i) % num_punters_;
      // TODO: Reset and run.
      GameMove next_move = punter_->Run();
      //Run(cur_moves ++ new_moves);
      new_moves.push_back(next_move);
    }
    std::vector<GameMove> next_moves(old.moves);
    copy(new_moves.begin(), new_moves.end(),
         std::back_inserter(next_moves));
    new_snapshots->push_back(GenerateSnapshot(next_moves));
  }
}

void SimulatingPunter::ShrinkToTop(std::vector<Snapshot>* snapshots) {
  const int kWidth = 1000;
  UniqifyByMove(snapshots);
  std::sort(snapshots->begin(), snapshots->end(),
            [](const Snapshot& lhs, const Snapshot& rhs) {
      return lhs.total_score < rhs.total_score;});
  std::reverse(snapshots->begin(), snapshots->end());
  if (snapshots->size() > kWidth) {
    snapshots->resize(kWidth);
  }
}

// void SimulatingPunter::SetState(std::unique_ptr<base::Value> state) {
//   SimplePunter::SetState(std::move(state));
//   // TODO: proto_->SetProtoState();
// }


}  // namespace punter
