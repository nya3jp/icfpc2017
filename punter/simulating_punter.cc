#include "punter/simulating_punter.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/ptr_util.h"
#include "punter/greedy_punter_mirac.h"

namespace punter {

using framework::GameMove;

struct Snapshot {
  // List of taken moves. Note: source and target are always sorted
  // by ClaimRiver.
  std::vector<GameMove> moves;
  std::string key;  // Generated from sorted move list.
  std::vector<int> scores;
  int total_score;
};

class Shadow : public GreedyPunterMirac {
public:
  Shadow() = default;
  ~Shadow() override = default;
  void Init(const framework::GameStateProto& proto) {
    auto next = base::MakeUnique<framework::GameStateProto>();
    next->CopyFrom(proto);
    SetStateFromProto(std::move(next));
  }

  std::unique_ptr<Shadow> Clone() const {
    auto res = base::MakeUnique<Shadow>();
    res->Init(proto_);
    return res;
  }
  void Advance(const GameMove& next) {
    GameMove m(next);
    InternalGameMoveToExternal(&m);
    SimplePunter::Run(std::vector<GameMove>{m});
  }
  GameMove NextMove(int punter_id) {
    int old_punter_id = punter_id_;
    punter_id_ = punter_id;
    GameMove r = Run();
    punter_id_ = old_punter_id;
    return r;
  }
};

namespace {

std::string MovesToKey(std::vector<GameMove> moves) {
  std::sort(moves.begin(), moves.end(),
            [](const GameMove& lhs, const GameMove& rhs) {
              return lhs.punter_id != rhs.punter_id ?
                lhs.punter_id < rhs.punter_id :
                lhs.source != rhs.source ? lhs.source < rhs.source :
                lhs.target < rhs.target; });
  // TODO: speed this up.
  std::ostringstream oss;
  for (const auto& m : moves) {
    oss << m.punter_id << " " << m.source << " " << m.target;
  }
  return oss.str();
}

int TotalScore(int punter_id, const std::vector<int>& scores) {
  const int my_score = scores[punter_id];
  int score_above = my_score - 1;
  int score_below = my_score + 1;
  for (size_t i = 0; i < scores.size(); ++i) {
    if (static_cast<int>(i) == punter_id) {continue;}
    const int s = scores[i];
    if (s < my_score) {
      if (score_below > my_score || score_below < s) {
        score_below = s;
      }
    } else if (s > my_score) {
      if (score_above < my_score || s < score_above) {
        score_above = s;
      }
    }
  }
  return (my_score - score_below) - (score_above - my_score);
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

std::string SnapshotScoreStr(const Snapshot& ss) {
  std::ostringstream oss;
  oss << "tt:" << ss.total_score << " score:";
  for (size_t i = 0; i < ss.scores.size(); ++i) {
    oss << ss.scores[i] << ",";
  }
  return oss.str();
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
  new_snapshots.reserve(snapshot_map.size());
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
}

framework::GameMove SimulatingPunter::Run() {
  const int kMaxStep = 3;
  std::vector<Snapshot> old_snapshot{GenerateSnapshot({})};
  for (int step = 0; step < kMaxStep; ++step) {
    VLOG(1) << "step:" << step << " Score:" << old_snapshot.front().total_score << "-" << old_snapshot.back().total_score << " (top:" << SnapshotScoreStr(old_snapshot.front()) << ") count: " << old_snapshot.size();
    std::vector<Snapshot> new_snapshots;
    for (const auto& state : old_snapshot) {
      GenerateNextSnapshots(state, &new_snapshots);
    }
    if (new_snapshots.size() == 0) {
      // Game end.
      break;
    }
    ShrinkToTop(&new_snapshots);
    old_snapshot.swap(new_snapshots);
  }
  return old_snapshot[0].moves[0];
}

Snapshot SimulatingPunter::GenerateSnapshot(
    const std::vector<GameMove>& moves) const {
  std::vector<int> scores = Simulate(moves);
  return SnapshotFromMove(punter_id_, moves, scores);
}

void SimulatingPunter::GenerateNextSnapshots(
    const Snapshot& old,
    std::vector<Snapshot>* new_snapshots) {
  std::unique_ptr<Shadow> punter0 = SummonShadow();
  for (const auto& m : old.moves) {
    punter0->Advance(m);
  }
  // Choose any river.
  for (const auto& r : *rivers_) {
    if (r.punter() >= 0)
      continue;
    std::unique_ptr<Shadow> punter = punter0->Clone();
    std::vector<GameMove> new_moves{ClaimRiver(punter_id_, r)};
    punter->Advance(new_moves[0]);
    for (int i = 1; i < num_punters_; ++i) {
      int cur_punter = (punter_id_ + i) % num_punters_;
      GameMove next_move = punter->NextMove(cur_punter);
      new_moves.push_back(next_move);
      punter->Advance(next_move);
    }
    std::vector<GameMove> next_moves(old.moves);
    copy(new_moves.begin(), new_moves.end(),
         std::back_inserter(next_moves));
    new_snapshots->push_back(GenerateSnapshot(next_moves));
  }
}

void SimulatingPunter::ShrinkToTop(std::vector<Snapshot>* snapshots) {
  const int kWidth = 100;
  UniqifyByMove(snapshots);
  std::sort(snapshots->begin(), snapshots->end(),
            [](const Snapshot& lhs, const Snapshot& rhs) {
      return lhs.total_score < rhs.total_score;});
  std::reverse(snapshots->begin(), snapshots->end());
  if (snapshots->size() > kWidth) {
    VLOG(1) << "Shrinking score:" << (*snapshots)[kWidth].total_score << "-" << snapshots->back().total_score << " size: " << snapshots->size();
    snapshots->resize(kWidth);
  }
}

std::unique_ptr<Shadow> SimulatingPunter::SummonShadow() const {
  auto shadow =  base::MakeUnique<Shadow>();
  shadow->Init(proto_);
  return shadow;
}

}  // namespace punter
