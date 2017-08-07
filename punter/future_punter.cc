#include "punter/future_punter.h"

#include <vector>
#include <queue>

#include "base/memory/ptr_util.h"
#include "common/scorer.h"
#include "framework/game_proto.pb.h"
#include "framework/simple_punter.h"
#include "gflags/gflags.h"

using framework::MineProto;
using framework::RiverProto;
using framework::FutureProto;

DEFINE_bool(future_aggressive, false, "");

namespace punter {

namespace {

struct FutureCandidate {
  int score;
  int dist;
  int mine;
  int site;
};

}

FuturePunter::FuturePunter() {}
FuturePunter::~FuturePunter() = default;

void FuturePunter::GenerateRiversToClaim() {
  std::vector<std::vector<std::pair<int, int>>> rivers_to_claim(
      num_sites(), std::vector<std::pair<int, int>>(
          mines_->size(), std::make_pair(-1, -1)));

  for (int i = 0; i < mines_->size(); ++i) {
    int mine = mines_->Get(i).site();

    // We may want to implement this with two queues.
    std::priority_queue<std::pair<int, int>> q;  // {(-num_rivers_to_claim, site_idx)}
    rivers_to_claim[mine][i] = std::make_pair(0, -1);
    q.push(std::make_pair(-0, mine));
    while(q.size()) {
      int dist = -q.top().first;
      int site = q.top().second;
      q.pop();
      for (Edge edge : edges_[site]) {
        int punter_of_edge = rivers_->Get(edge.river).punter();
        // Ignore the river if it's claimed by somebody else.
        if (punter_of_edge != -1 && punter_of_edge != punter_id_) continue;
        int next_site = edge.site;
        int next_dist = dist + (punter_of_edge == -1);
        if (rivers_to_claim[next_site][i].first != -1 &&
            rivers_to_claim[next_site][i].first <= next_dist) continue;
        rivers_to_claim[next_site][i] = std::make_pair(next_dist, site);
        q.push(std::make_pair(-next_dist, next_site));
      }
    }
    for (int k = 0; k < num_sites(); ++k) {
      VLOG(10) << " from " << mine << " to " << k << ": distance:" << rivers_to_claim[k][i].first << " via: " << rivers_to_claim[k][i].second;
    }
  }
  rivers_to_claim_.swap(rivers_to_claim);
}

std::vector<framework::Future> FuturePunter::GetFuturesImpl() {
  GenerateRiversToClaim();

  std::vector<FutureCandidate> candidates;

  candidates.reserve(mines_->size());
  for (int k = 0; k < mines_->size(); ++k) {
    int max_score = -1;
    int min_dist = -1;
    int max_site_idx = -1;
    int max_mine_idx = -1;
    for (int i = 0; i < num_sites(); ++i) {
      if (rivers_to_claim_[i][k].first == -1) continue;
      if (rivers_to_claim_[i][k].first == 0) continue;
      int score = dist_to_mine(i, k);
      int dist = rivers_to_claim_[i][k].first;
      if (max_score < score ||
          (max_score == score && dist < min_dist)) {
        max_score = score;
        min_dist = dist;
        max_site_idx = i;
        max_mine_idx = k;
      }
    }
    // Target must be a non-mine site.
    if (std::find_if(mines_->begin(), mines_->end(),
                     [max_site_idx](const MineProto& mine) {
                       return mine.site() == max_site_idx;
                     }) == mines_->end()) {
      candidates.emplace_back(FutureCandidate({max_score, min_dist, max_mine_idx, max_site_idx}));
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const FutureCandidate& a, const FutureCandidate& b) { return (a.score > b.score); });

  for (int i = 0; i < candidates.size(); i++) {
    FutureProto* future_proto = proto_.add_futures();
    future_proto->set_mine_index(candidates[i].mine);
    future_proto->set_target(candidates[i].site);
    DLOG(INFO) << "future: " << mines_->Get(candidates[i].mine).site() << "-" << candidates[i].site;
    if (!FLAGS_future_aggressive)
      break;
  }

  std::vector<framework::Future> futures;
  for (auto& f : proto_.futures())
    futures.emplace_back(framework::Future({mines_->Get(f.mine_index()).site(), f.target()}));
  return futures;
}

framework::GameMove FuturePunter::Run() {
  common::Scorer scorer(proto_.mutable_scorer());
  GenerateRiversToClaim();

  for (auto& f : proto_.futures()) {
    int cost = rivers_to_claim_[f.target()][f.mine_index()].first;
    if (cost > 0)
      return TryToConnect(f.mine_index(), f.target());
  }
  return RunMirac();
}

framework::GameMove FuturePunter::RunMirac() {
  int max_score = -1;
  int min_dist = -1;
  int max_site_idx = -1;
  int max_mine_idx = -1;
  for (int i = 0; i < num_sites(); ++i) {
    for (int k = 0; k < mines_->size(); ++k) {
      if (rivers_to_claim_[i][k].first == -1) continue;
      if (rivers_to_claim_[i][k].first == 0) continue;
      int score = dist_to_mine(i, k);
      int dist = rivers_to_claim_[i][k].first;
      if (max_score < score ||
          (max_score == score && dist < min_dist)) {
        max_score = score;
        min_dist = dist;
        max_site_idx = i;
        max_mine_idx = k;
      }
    }
  }
  VLOG(10) << "max score: " << max_score << " min_dist: " << min_dist << " max_site_idx: " << max_site_idx << " max_mine_idx: " << max_mine_idx;

  if (max_score == -1) {
    // We cannot gain more points, but claim edge to disturb other punters.
    for (const RiverProto& river : *rivers_) {
      if (river.punter() == -1) {
          return {framework::GameMove::Type::CLAIM, punter_id_, river.source(), river.target()};
      }
    }
    return {framework::GameMove::Type::PASS, punter_id_};
  }
  return TryToConnect(max_mine_idx, max_site_idx);
}

framework::GameMove FuturePunter::TryToConnect(int mine, int site) {
  int prev_pos = -1;
  int pos = site;
  while (1) {
    std::pair<int, int> data = rivers_to_claim_[pos][mine];
    if (data.first == 0) break;
    prev_pos = pos;
    pos = data.second;
  }

  return {framework::GameMove::Type::CLAIM, punter_id_, pos, prev_pos};
}

} // namespace punter
