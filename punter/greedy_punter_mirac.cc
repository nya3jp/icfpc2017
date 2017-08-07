#include "punter/greedy_punter_mirac.h"

#include <queue>

#include "base/memory/ptr_util.h"
#include "framework/game_proto.pb.h"
#include "framework/simple_punter.h"
#include "gflags/gflags.h"

using framework::RiverProto;

DEFINE_bool(use_option, false, "");

namespace punter {

GreedyPunterMirac::GreedyPunterMirac() {}
GreedyPunterMirac::~GreedyPunterMirac() = default;

struct Cost {
  int options;
  int rivers;
  int prev_site;
  bool use_option;
};
struct Candidate {
  int options;
  int rivers;
  int site;
  bool operator < (const Candidate& other) const {
    if (options != other.options) {
      return options > other.options;
    }
    return rivers > other.rivers;
  }
};

framework::GameMove GreedyPunterMirac::Run() {
  size_t num_mines = mines_->size();

  // site_idx -> mine_idx -> (Cost)
  std::vector<std::vector<Cost>> rivers_to_claim(
      num_sites(), std::vector<Cost>(
          num_mines, Cost{-1, -1, -1, false}));

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = mines_->Get(i).site();

    // We may want to implement this with two queues.
    std::priority_queue<Candidate> q;
    rivers_to_claim[mine][i] = Cost{0, 0, -1, false};
    q.push(Candidate{0, 0, mine});
    while(q.size()) {
      int options = q.top().options;
      int remaining_options =
          (FLAGS_use_option ?  GetOptionsRemaining() : 0) - options;
      int dist = q.top().rivers;
      int site = q.top().site;
      q.pop();
      for (Edge edge : edges_[site]) {
        int punter_of_edge = rivers_->Get(edge.river).punter();
        int option_punter_of_edge = rivers_->Get(edge.river).option_punter();
        bool need_option = (punter_of_edge != -1 &&
                            punter_of_edge != punter_id_);
        if (need_option && remaining_options == 0) continue;
        if (need_option && option_punter_of_edge != -1) continue;
        int next_option = options + need_option;
        int next_dist = dist + (punter_of_edge != punter_id_);
        int next_site = edge.site;
        if (rivers_to_claim[next_site][i].rivers != -1 &&
            (rivers_to_claim[next_site][i].options < next_option ||
             (rivers_to_claim[next_site][i].options == next_option &&
              rivers_to_claim[next_site][i].rivers <= next_dist))) continue;
        rivers_to_claim[next_site][i] = Cost{next_option, next_dist, site, need_option};
        q.push(Candidate{next_option, next_dist, next_site});
      }
    }
    for (int k = 0; k < num_sites(); ++k) {
      VLOG(10) << " from " << mine << " to " << k
               << ": distance:" << rivers_to_claim[k][i].rivers
               << " via: " << rivers_to_claim[k][i].prev_site
               << " with: " << rivers_to_claim[k][i].use_option;
    }
  }

  int max_score = -1;
  int min_options = -1;
  int min_rivers = -1;
  int max_site_idx = -1;
  int max_mine_idx = -1;
  for (int i = 0; i < num_sites(); ++i) {
    for (size_t k = 0; k < num_mines; ++k) {
      if (rivers_to_claim[i][k].rivers == -1) continue;
      if (rivers_to_claim[i][k].rivers == 0) continue;
      int score = dist_to_mine(i, k);
      int options = rivers_to_claim[i][k].options;
      int rivers = rivers_to_claim[i][k].rivers;
      if (max_score < score ||
          (max_score == score && min_options < options) ||
          (max_score == score && min_options == options && rivers < min_rivers)) {
        max_score = score;
        min_options = options;
        min_rivers = rivers;
        max_site_idx = i;
        max_mine_idx = k;
      }
    }
  }
  VLOG(10) << "max score: " << max_score << "min_options: " << min_options
           << " min_rivers: " << min_rivers << " max_site_idx: " << max_site_idx
           << " max_mine_idx: " << max_mine_idx;

  if (max_score == -1) {
    // We cannot gain more points, but claim edge to disturb other punters.
    for (const RiverProto& river : *rivers_) {
      if (river.punter() == -1) {
          return {framework::GameMove::Type::CLAIM, punter_id_, river.source(), river.target()};
      }
    }
    return {framework::GameMove::Type::PASS, punter_id_};
  }

  int prev_pos = -1;
  int pos = max_site_idx;
  bool prev_use_option = false;
  while (1) {
    Cost data = rivers_to_claim[pos][max_mine_idx];
    if (data.rivers == 0) break;
    prev_pos = pos;
    prev_use_option = data.use_option;
    pos = data.prev_site;
  }

  return {prev_use_option ? framework::GameMove::Type::OPTION
                          : framework::GameMove::Type::CLAIM,
          punter_id_, pos, prev_pos};
}

} // namespace punter
