#include "punter/friendly_punter.h"

#include <vector>
#include <random>
#include <queue>

#include "base/memory/ptr_util.h"
#include "framework/game_proto.pb.h"

using framework::RiverProto;

namespace punter {

namespace {

const int INF = 1000000000;

void dfs(int site_idx,
         std::vector<bool>& visited,
         const std::vector<std::vector<int>>& adj,
         const std::vector<std::vector<int>>& adj2) {
  if (visited[site_idx])
    return;
  
  visited[site_idx] = true;
  for (size_t i = 0; i < adj[site_idx].size(); i++)
    dfs(adj[site_idx][i], visited, adj, adj2);
  for (size_t i = 0; i < adj2[site_idx].size(); i++)
    dfs(adj2[site_idx][i], visited, adj, adj2);
}

void dfs(int site_idx,
         const std::vector<std::vector<int>>& adj,
         std::vector<bool>& visited) {
  if (visited[site_idx])
    return;

  visited[site_idx] = true;
  for (size_t i = 0; i < adj[site_idx].size(); i++)
    dfs(adj[site_idx][i], adj, visited);
}

}  // namespace

FriendlyPunter::FriendlyPunter() = default;
FriendlyPunter::~FriendlyPunter() = default;

framework::GameMove FriendlyPunter::Run() {
  const int S = edges_.size();
  const int M = mines_->size();

  int remaining_moves = (num_remaining_turns() - 1) / num_punters_;
  if (CanOption() && GetOptionsRemaining() >= remaining_moves) {
    return TryReplace();
  }

  // value[site][mine]: value of site from mine.
  std::vector<std::vector<int>> value(S, std::vector<int>(M, 0));
  for (int i=0; i<S; i++) {
    for (int j=0; j<M; j++) {
      value[i][j] += dist_to_mine(i, j) * dist_to_mine(i, j);
    }
  }
  
  // Constract owned subgraph.
  std::vector<std::vector<int>> adj(S, std::vector<int>());
  for (auto& r : *rivers_) {
    if (r.punter() != punter_id_)
      continue;
    adj[r.source()].push_back(r.target());
    adj[r.target()].push_back(r.source());
  }

  // Constract adjacent list of reachabel (not-owned-by-others) edges.
  std::vector<std::vector<int>> adj_available(S, std::vector<int>());
  for (auto& r : *rivers_) {
    if (!(r.punter() == punter_id_ || r.punter() == -1))
      continue;
    adj_available[r.source()].push_back(r.target());
    adj_available[r.target()].push_back(r.source());
  }

  std::vector<std::vector<bool>> covered(M, std::vector<bool>(S, false));
  for (int i=0; i<M; i++)
    dfs(mines_->Get(i).site(), adj, covered[i]);
  
  // Compute score from each mine.
  std::vector<int> mine_effect(M, 0);
  for (int i=0; i<M; i++) {
    std::vector<bool> visited(S, false);
    dfs(mines_->Get(i).site(), adj, visited);
    for (int j=0; j<S; j++) {
      if (visited[j]) {
        mine_effect[i] += value[j][i];
      }
    }
  }
  // Compute max score from each mine.
  std::vector<int> max_sites(M, 0);
  for (int i=0; i<M; i++) {
    std::vector<bool> visited(S, false);
    dfs(mines_->Get(i).site(), adj_available, visited);
    for (int j=0; j<S; j++) {
      if (visited[j])
        max_sites[i]++;
    }
  }

  // Find the least effective mine among the ones which have maximum max_score.
  std::vector<std::pair<std::pair<int,int>, int>> vp;
  for (int i=0; i<M; i++) {
    vp.push_back(std::make_pair(std::make_pair(max_sites[i], -mine_effect[i]), i));
  }
  sort(vp.rbegin(), vp.rend());

  for (size_t i=0; i<vp.size(); i++) {
    std::pair<int, int> result = FindForMine(vp[i].second, adj, adj_available, value, covered);
    if (result.first >= 0)
      return CreateClaim(result.first, result.second);
  }
  std::vector<std::pair<double, int>> river_candidates;
  for (int i=0; i<rivers_->size(); i++) {
    if (rivers_->Get(i).punter() == -1) {
      int src = rivers_->Get(i).source();
      int tgt = rivers_->Get(i).target();
      double score = 0;
      for (int j=0; j<M; j++) {
        if (covered[j][src] ^ covered[j][tgt]) {
          score += value[src][j] * value[src][j];
          score += value[tgt][j] * value[tgt][j];
        }
        if (covered[j][src] || covered[j][tgt]) {
          score += value[src][j];
          score += value[tgt][j];
        }
      }
      river_candidates.push_back(std::make_pair(score, i));
    }
  }
  sort(river_candidates.rbegin(), river_candidates.rend());
  if (river_candidates.size() > 0) {
    int idx = river_candidates[0].second;
    int src = rivers_->Get(idx).source();
    int tgt = rivers_->Get(idx).target();
    return CreateClaim(src, tgt);
  }
  return CreatePass();
}

void FriendlyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> FriendlyPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  return state;
}

std::pair<int, int> FriendlyPunter::FindForMine(
    int mine_index,
    const std::vector<std::vector<int>>& adj,
    const std::vector<std::vector<int>>& adj_available,
    const std::vector<std::vector<int>>& value,
    const std::vector<std::vector<bool>>& covered) {
  const int S = edges_.size();
  const int M = mines_->size();

  // Compute covered nodes from any of mines.
  std::vector<bool> covered_by_any(S, false);
  for (int i=0; i<M; i++)
    dfs(mines_->Get(i).site(), adj, covered_by_any);

  // BFS from the least effective mine.
  std::vector<bool> visited(S, false);
  std::vector<int> distance(S, INF);
  dfs(mines_->Get(mine_index).site(), adj, visited);
  std::vector<bool> covered_by_mine = visited;
  std::queue<std::pair<int, int>> q;
  for (int i=0; i<S; i++) if (visited[i]) {
    distance[i] = 0;
    q.push(std::make_pair(i, 0));
  }
  while (!q.empty()) {
    int v = q.front().first;
    int d = q.front().second;
    q.pop();
    for (Edge& e : edges_[v]) {
      if (visited[e.site])
        continue;
      if (rivers_->Get(e.river).punter() != -1)
        continue;
      int nv = e.site;
      int nd = d + 1;
      visited[nv] = true;
      distance[nv] = nd;
      q.push(std::make_pair(nv, nd));
    }
  }

  // Determine set of reachable nodes from the mine.
  std::vector<bool> reachable(S, false);
  dfs(mines_->Get(mine_index).site(), adj_available, reachable);

  // Compute site's value
  std::vector<double> site_values(S, 0);
  for (int i=0; i<S; i++) {
    if (covered[mine_index][i])
      continue;
    if (!reachable[i])
      continue;

    if (covered_by_any[i])
      site_values[i] += 100000000000000.0 / distance[i];

    int available_otonari = 0;
    for (Edge& e : edges_[i]) {
      if (visited[e.site])
        continue;
      if (rivers_->Get(e.river).punter() != -1 &&
          rivers_->Get(e.river).punter() != punter_id_)
        continue;
      available_otonari++;
    }
    site_values[i] += accumulate(value[i].begin(), value[i].end(), 0) * (1 + available_otonari);
  }

  // Determine the target site.
  int best_target = -1;
  double best_score = -1;
  for (int i=0; i<S; i++) {
    if (best_score < site_values[i]) {
      best_score = site_values[i];
      best_target = i;
    }
  }

  if (best_score == 0) {
    return std::make_pair(-1, -1);
  }

  // Construct path from target site to the least effective mine's tree.
  int v = best_target;
  int d = distance[best_target];
  std::vector<std::pair<int, int>> path;
  while (d > 0) {
    for (Edge& e : edges_[v]) {
      if (rivers_->Get(e.river).punter() != -1)
        continue;

      int nv = e.site;
      if (distance[nv] == d-1) {
        path.push_back(std::make_pair(v, nv));
        d = d-1;
        v = nv;
        break;
      }
    }
  }
  if (path.empty())
    return std::make_pair(-1, -1);

  int src = path.back().first;
  int tgt = path.back().second;
  // Claim the river of the last river in the constructed path.
  return std::make_pair(src, tgt);
}

framework::GameMove FriendlyPunter::TryReplace() {
  int best_score = -1;
  int best_river_index = -1;
  for (int i = 0; i < rivers_->size(); i++) {
    if (rivers_->Get(i).punter() != punter_id_ && rivers_->Get(i).punter() != -1 &&
        rivers_->Get(i).option_punter() != punter_id_ && rivers_->Get(i).option_punter() != -1) {
      continue;
    }
    
    int score = TryClaim(punter_id_, rivers_->Get(i).source(), rivers_->Get(i).target());
    if (score > best_score) {
      best_score = score;
      best_river_index = i;
    }
  }

  if (best_river_index == -1)
    return CreatePass();

  if (rivers_->Get(best_river_index).punter() == -1)
    return CreateClaim(rivers_->Get(best_river_index).source(), rivers_->Get(best_river_index).target());
  else
    return CreateOption(rivers_->Get(best_river_index).source(), rivers_->Get(best_river_index).target());
}

}  // namespace punter

