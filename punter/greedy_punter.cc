#include "punter/greedy_punter.h"

#include <queue>
#include "base/memory/ptr_util.h"

namespace punter {

GreedyPunter::GreedyPunter() = default;
GreedyPunter::~GreedyPunter() = default;

void GreedyPunter::SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) {
  SimplePunter::SetUp(punter_id, num_punters, game_map);

  connected_from_mine_.resize(mines_.size());
  for (size_t i = 0; i < mines_.size(); i++) {
    connected_from_mine_[i].insert(mines_[i]);
  }
  ComputeLongestPath();
}

std::vector<framework::Future> GreedyPunter::GetFutures() {
  std::vector<framework::Future> futures;
  int source = longest_path_[0];
  // Target is the farthest non-mine site
  for (int i = longest_path_.size() - 1; i > 0; i--) {
    int target = longest_path_[i];
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

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = site_id_to_site_idx_[mines_[i]];

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
      longest_path_.resize(length + 1);

      int site = last;
      for (int i = 0; i < length; i++) {
        longest_path_[length - i] = sites_[site].id;
        site = dist_and_prev[site].second;
      }
      longest_path_[0] = sites_[site].id;
    }
  }
}

framework::GameMove GreedyPunter::Run() {
  const RiverWithPunter* river_with_max_score = nullptr;
  int max_score = -1;
  std::unique_ptr<std::set<std::pair<int, int>>> max_mines;  // {(mine_idx, site_id)}
  int longest_src = 0, longest_target = 0;
  if (longest_path_index_ < static_cast<int>(longest_path_.size()) - 1) {
    longest_src = longest_path_[longest_path_index_];
    longest_target = longest_path_[longest_path_index_ + 1];
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

      int dist = dist_to_mine_[site_id_to_site_idx_[target]][mine_id];
      score += dist * dist;
      mines->insert(std::make_pair(mine_id, target));
    }
    DLOG(INFO) << r.source << " -> " << r.target << ": " << score;
    if ((r.source == longest_src && r.target == longest_target) ||
        (r.target == longest_src && r.source == longest_target)) {
      longest_path_index_++;
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
    }
    return {framework::GameMove::Type::CLAIM, punter_id_,
        river_with_max_score->source, river_with_max_score->target};
  }
  return {framework::GameMove::Type::PASS, punter_id_};
}

void GreedyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  base::ListValue* value;
  CHECK(state->GetList("greedy", &value));
  CHECK(state->GetInteger("longest_path_index", &longest_path_index_));

  connected_from_mine_.resize(value->GetSize());
  for (size_t mine_id = 0; mine_id < connected_from_mine_.size(); mine_id++) {
    base::ListValue* v;
    CHECK(value->GetList(mine_id, &v));
    for (size_t i = 0; i < v->GetSize(); i++) {
      int site;
      CHECK(v->GetInteger(i, &site));
      connected_from_mine_[mine_id].insert(site);
    }
  }

  SimplePunter::SetState(std::move(state));
  ComputeLongestPath();
}

std::unique_ptr<base::Value> GreedyPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());

  auto value = base::MakeUnique<base::ListValue>();
  value->Reserve(mines_.size());
  for (size_t mine_id = 0; mine_id < mines_.size(); mine_id++) {
    auto v = base::MakeUnique<base::ListValue>();
    v->Reserve(connected_from_mine_[mine_id].size());
    for (int site : connected_from_mine_[mine_id]) {
      v->AppendInteger(site);
    }
    value->Append(std::move(v));
  }

  state->Set("greedy", std::move(value));
  state->SetInteger("longest_path_index", longest_path_index_);
  return state;
}

} // namespace punter
