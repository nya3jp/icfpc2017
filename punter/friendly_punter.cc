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
  
  // Compute covered nodes from any of mines.
  std::vector<bool> covered(S, false);
  for (int i=0; i<M; i++)
    dfs(mines_->Get(i).site(), adj, covered);

  // Compute score from each mine.
  std::vector<int> mine_effect(M, 0);
  for (int i=0; i<M; i++) {
    std::vector<bool> visited(S, false);
    dfs(mines_->Get(i).site(), adj, visited);
    for (int j=0; j<S; j++) {
      if (visited[j])
        mine_effect[i] += value[j][i];
    }
  }
  
  // Find the least effective mine.
  int least_effective_mine = -1;
  int least_effectiveness = INF;
  for (int i=0; i<M; i++) {
    if (mine_effect[i] < least_effectiveness) {
      least_effective_mine = i;
      least_effectiveness = mine_effect[i];
    }
  }

  // BFS from the least effective mine.
  std::vector<bool> visited(S, false);
  std::vector<int> distance(S, INF);
  dfs(mines_->Get(least_effective_mine).site(), adj, visited);
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
  std::vector<std::vector<int>> adj2(S, std::vector<int>());
  for (auto& r : *rivers_) {
    if (r.punter() == punter_id_ || r.punter() == -1) {
      adj2[r.source()].push_back(r.target());
      adj2[r.target()].push_back(r.source());
    }
  }
  std::vector<bool> reachable(S, false);
  dfs(mines_->Get(least_effective_mine).site(), adj2, reachable);

  // Compute site's value
  std::vector<int> site_values(S, 0);
  for (int i=0; i<S; i++) {
    if (covered_by_mine[i])
      continue;
    if (!reachable[i])
      continue;

    if (covered[i])
      site_values[i] += 1000000 - distance[i];

    int available_otonari = 0;
    for (Edge& e : edges_[i]) {
      if (visited[e.site])
        continue;
      if (rivers_->Get(e.river).punter() != -1 &&
          rivers_->Get(e.river).punter() != punter_id_)
        continue;
      available_otonari++;
    }
    site_values[i] += accumulate(value[i].begin(), value[i].end(), 0) * (1 + available_otonari / 2);
  }

  // Determine the target site.
  int best_target = -1;
  int best_score = -1;
  for (int i=0; i<S; i++) {
    if (best_score < site_values[i]) {
      best_score = site_values[i];
      best_target = i;
    }
  }

  if (best_score == 0)
    return CreatePass();

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
    return CreatePass();

  int src = path.back().first;
  int tgt = path.back().second;
  // Claim the river of the last river in the constructed path.
  return CreateClaim(src, tgt);
}

void FriendlyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> FriendlyPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  return state;
}

std::vector<double> FriendlyPunter::ComputeReachability(
    int mine_idx,
    const std::vector<std::pair<int, int>>& remaining_edges,
    const std::vector<std::vector<int>>& adj) {
  std::random_device rd;
  std::mt19937 g(rd());

  std::vector<double> p(edges_.size(), 0.0);
  const int kIterations = 30;
  std::vector<std::pair<int, int>> edges2 = remaining_edges;
  for (size_t t = 0; t < kIterations; t++) {
    
    // Randomly choose remaining edges
    std::shuffle(edges2.begin(), edges2.end(), g);

    // Construct graph
    std::vector<std::vector<int>> adj2(edges_.size(), std::vector<int>());
    for (size_t j = 0; j < edges2.size() / num_punters_; j++) {
      adj2[edges2[j].first].push_back(edges2[j].second);
      adj2[edges2[j].second].push_back(edges2[j].first);
    }

    // Fill |visited| by dfs from mine
    std::vector<bool> visited(edges_.size(), false);
    int start_idx = mines_->Get(mine_idx).site();
    dfs(start_idx, visited, adj, adj2);

    // Count up 
    for (size_t j = 0; j < edges_.size(); j++)
      p[j] += visited[j] ? 1 : 0;
  }

  for (size_t i = 0; i < p.size(); i++)
    p[i] /= kIterations;

  return p;
}

double FriendlyPunter::Evaluate() {
  std::vector<std::vector<int>> adj(edges_.size(), std::vector<int>());
  int hoge = 0;
  for (auto& r : *rivers_) {
    if (r.punter() != punter_id_)
      continue;
    hoge++;
    adj[r.source()].push_back(r.target());
    adj[r.target()].push_back(r.source());
  }

  int fuga = 0;
  std::vector<std::pair<int, int>> remaining_edges;
  for (auto& r : *rivers_) {
    if (r.punter() != -1)
      continue;
    fuga++;
    remaining_edges.push_back(std::make_pair(r.source(), r.target()));
  }

  std::vector<double> scores(edges_.size(), 0.0);
  for (int mine_index = 0; mine_index < mines_->size(); mine_index++) {
    std::vector<double> p = ComputeReachability(mine_index, remaining_edges, adj);
    for (size_t i = 0; i < edges_.size(); i++) {
      int dist = dist_to_mine(i, mine_index);
      scores[i] += dist * dist * p[i];
    }
  }
  return std::accumulate(scores.begin(), scores.end(), 0.0);
}

}  // namespace punter

