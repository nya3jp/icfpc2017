#include "punter/greedy_punter.h"

#include <queue>
#include "base/memory/ptr_util.h"
#include "punter/greedy_punter.pb.h"

namespace punter {

GreedyPunter::GreedyPunter() = default;
GreedyPunter::~GreedyPunter() = default;

void GreedyPunter::SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) {
  SimplePunter::SetUp(punter_id, num_punters, game_map);

  ComputeLongestPath();

  auto greedy_ext = proto_.MutableExtension(GreedyPunterProto::greedy_ext);
  greedy_ext->set_longest_path_index(0);

  connected_from_mine_.resize(mines_.size());
  for (size_t i = 0; i < mines_.size(); i++) {
    SiteSetProto* site_set = greedy_ext->add_connected_from_mine();
    site_set->add_site(mines_[i]);

    connected_from_mine_[i].insert(mines_[i]);
  }
}

std::vector<framework::Future> GreedyPunter::GetFutures() {
  std::vector<framework::Future> futures;

  auto greedy_ext = proto_.MutableExtension(GreedyPunterProto::greedy_ext);
  int source = greedy_ext->longest_path(0);
  // Target is the farthest non-mine site
  for (int i = greedy_ext->longest_path_size() - 1; i > 0; i--) {
    int target = greedy_ext->longest_path(i);
    if (std::find(mines_.begin(), mines_.end(), target) == mines_.end()) {
      futures.push_back({source, target});
      break;
    }
  }
  return futures;
}

void GreedyPunter::ComputeLongestPath() {
  size_t num_sites = sites_.size();
  size_t num_mines = mines_.size();

  int max_length = 0;
  std::vector<int> longest_path;

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = mines_[i];

    std::vector<std::pair<int, int>> dist_and_prev; // site_idx -> (distance, site_idx)
    dist_and_prev.resize(num_sites, std::make_pair(-1, -1));

    std::queue<std::pair<int, int>> q;  // {(site_idx, dist)}
    dist_and_prev[mine] = std::make_pair(0, -1);
    q.push(std::make_pair(mine, 0));
    int length = 0, last = 0;
    while (q.size()) {
      int site = q.front().first;
      int dist = q.front().second;
      q.pop();
      for (Edge edge : edges_[site]) {
        int next = edge.site;
        if (dist_and_prev[next].first != -1) continue;
        dist_and_prev[next] = std::make_pair(dist + 1, site);
        q.push(std::make_pair(next, dist + 1));
        length = dist + 1;
        last = next;
      }
    }
    if (length > max_length) {
      max_length = length;
      longest_path.resize(length + 1);

      int site = last;
      for (int i = 0; i < length; i++) {
        longest_path[length - i] = site;
        site = dist_and_prev[site].second;
      }
      longest_path[0] = site;
    }
  }
  auto greedy_ext = proto_.MutableExtension(GreedyPunterProto::greedy_ext);
  for (auto site : longest_path) {
    greedy_ext->add_longest_path(site);
  }
}

framework::GameMove GreedyPunter::Run() {
  auto greedy_ext = proto_.MutableExtension(GreedyPunterProto::greedy_ext);

  const RiverWithPunter* river_with_max_score = nullptr;
  int max_score = -1;
  std::unique_ptr<std::set<std::pair<int, int>>> max_mines;  // {(mine_idx, site_id)}
  int longest_src = 0, longest_target = 0;
  int longest_path_index = greedy_ext->longest_path_index();
  if (longest_path_index < greedy_ext->longest_path_size() - 1) {
    longest_src = greedy_ext->longest_path(longest_path_index);
    longest_target = greedy_ext->longest_path(longest_path_index + 1);
  }

  for (auto& r : rivers_) {
    if (r.punter != -1)
      continue;

    int score = 0;
    auto mines = base::MakeUnique<std::set<std::pair<int, int>>>();
    for (size_t mine_id = 0; mine_id < mines_.size(); mine_id++) {
      int target;
      if (connected_from_mine_[mine_id].count(r.source) && !connected_from_mine_[mine_id].count(r.target)) {
        target = r.target;
      } else if (!connected_from_mine_[mine_id].count(r.source) && connected_from_mine_[mine_id].count(r.target)) {
        target = r.source;
      } else {
        continue;
      }

      int dist = dist_to_mine_[target][mine_id];
      score += dist * dist;
      mines->insert(std::make_pair(mine_id, target));
    }
    DLOG(INFO) << r.source << " -> " << r.target << ": " << score;
    if ((r.source == longest_src && r.target == longest_target) ||
        (r.target == longest_src && r.source == longest_target)) {
      greedy_ext->set_longest_path_index(longest_path_index + 1);
      max_score = score;
      river_with_max_score = &r;
      max_mines = std::move(mines);
      break;
    }

    if (score > max_score) {
      max_score = score;
      river_with_max_score = &r;
      max_mines = std::move(mines);
    }
  }

  if (river_with_max_score) {
    for (auto pair : *max_mines) {
      connected_from_mine_[pair.first].insert(pair.second);
      greedy_ext->mutable_connected_from_mine(pair.first)->add_site(pair.second);
    }
    return {framework::GameMove::Type::CLAIM, punter_id_,
        river_with_max_score->source, river_with_max_score->target};
  }
  return {framework::GameMove::Type::PASS, punter_id_};
}

void GreedyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  SimplePunter::SetState(std::move(state_in));

  auto greedy_ext = proto_.MutableExtension(GreedyPunterProto::greedy_ext);
  connected_from_mine_.resize(greedy_ext->connected_from_mine_size());
  for (size_t i = 0; i < connected_from_mine_.size(); i++) {
    for (auto site : greedy_ext->connected_from_mine(i).site())
      connected_from_mine_[i].insert(site);
  }
}

} // namespace punter
