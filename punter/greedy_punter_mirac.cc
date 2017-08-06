#include "punter/greedy_punter_mirac.h"

#include <queue>

#include "base/memory/ptr_util.h"
#include "framework/simple_punter.h"

namespace punter {

GreedyPunterMirac::GreedyPunterMirac() {}
GreedyPunterMirac::~GreedyPunterMirac() = default;

framework::GameMove GreedyPunterMirac::Run() {
  size_t num_sites = sites_.size();
  size_t num_mines = mines_.size();

  // site_idx -> mine_idx -> (how many rivers do we need to
  // claim in order to connect the site and the mine, site_idx of
  // previous location)
  std::vector<std::vector<std::pair<int, int>>> rivers_to_claim(
      num_sites, std::vector<std::pair<int, int>>(
          num_mines, std::make_pair(-1, -1)));

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = mines_[i];

    // We may want to implement this with two queues.
    std::priority_queue<std::pair<int, int>> q;  // {(-num_rivers_to_claim, site_idx)}
    rivers_to_claim[mine][i] = std::make_pair(0, -1);
    q.push(std::make_pair(-0, mine));
    while(q.size()) {
      int dist = -q.top().first;
      int site = q.top().second;
      q.pop();
      for (Edge edge : edges_[site]) {
        int punter_of_edge = rivers_[edge.river].punter;
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
    for (size_t k = 0; k < num_sites; ++k) {
      VLOG(10) << " from " << mine << " to " << k << ": distance:" << rivers_to_claim[k][i].first << " via: " << rivers_to_claim[k][i].second;
    }
  }

  int max_score = -1;
  int min_dist = -1;
  int max_site_idx = -1;
  int max_mine_idx = -1;
  for (size_t i = 0; i < num_sites; ++i) {
    for (size_t k = 0; k < num_mines; ++k) {
      if (rivers_to_claim[i][k].first == -1) continue;
      if (rivers_to_claim[i][k].first == 0) continue;
      int score = dist_to_mine_[i][k];
      int dist = rivers_to_claim[i][k].first;
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
    for (const RiverWithPunter& river : rivers_) {
      if (river.punter == -1) {
          return {framework::GameMove::Type::CLAIM, punter_id_, river.source, river.target};
      }
    }
  }

  int prev_pos = -1;
  int pos = max_site_idx;
  while (1) {
    std::pair<int, int> data = rivers_to_claim[pos][max_mine_idx];
    if (data.first == 0) break;
    prev_pos = pos;
    pos = data.second;
  }

  return {framework::GameMove::Type::CLAIM, punter_id_, pos, prev_pos};
}

} // namespace punter
