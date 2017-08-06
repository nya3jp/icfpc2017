#include "punter/lazy_punter.h"

#include <vector>
#include <random>

#include "base/memory/ptr_util.h"
#include "framework/game_proto.pb.h"

using framework::RiverProto;

namespace punter {

namespace {

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


}  // namespace

LazyPunter::LazyPunter() = default;
LazyPunter::~LazyPunter() = default;

framework::GameMove LazyPunter::Run() {
  std::vector<std::pair<int, int>> remaining_edges;
  for (int i = 0; i < rivers_->size(); i++) {
    if (rivers_->Get(i).punter() == -1)
      remaining_edges.push_back(std::make_pair(rivers_->Get(i).source(), rivers_->Get(i).target()));
  }

  double best_score = -1;
  RiverProto best_river;
  for (auto& r : *rivers_) {
    if (r.punter() != -1)
      continue;

    r.set_punter(punter_id_);
    int score = Evaluate();
    r.set_punter(-1);

    if (score > best_score) {
      best_score = score;
      best_river = r;
    }
  }
  
  if (best_score == -1)
    return {framework::GameMove::Type::PASS, punter_id_};

  return {framework::GameMove::Type::CLAIM, punter_id_,
          best_river.source(), best_river.target()};
}

void LazyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> LazyPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  return state;
}

std::vector<double> LazyPunter::ComputeReachability(
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

double LazyPunter::Evaluate() {
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
