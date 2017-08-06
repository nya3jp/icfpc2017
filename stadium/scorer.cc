#include "stadium/scorer.h"

#include <algorithm>
#include <utility>
#include <queue>

#include "base/logging.h"
#include "stadium/punter.h"

namespace stadium {

namespace {

std::vector<int> CreateSiteIdList(const std::vector<Site>& sites) {
  std::vector<int> result;
  result.reserve(sites.size());
  for (const auto& site : sites) {
    result.push_back(site.id);
  }
  std::sort(result.begin(), result.end());
  return result;
}

// site_id_list must be sorted array.
int GetSiteIndex(const std::vector<int>& site_id_list, int site_id) {
  auto it =
      std::lower_bound(site_id_list.begin(), site_id_list.end(), site_id);
  DCHECK_EQ(*it, site_id);
  return static_cast<int>(std::distance(site_id_list.begin(), it));
}

std::vector<int> CreateMineList(const std::vector<Site>& sites,
                                const std::vector<int>& site_id_list) {
  std::vector<int> result;
  for (const auto& site : sites) {
    if (site.is_mine)
      result.push_back(GetSiteIndex(site_id_list, site.id));
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::pair<int, int>> CreateBidirectionalGraph(
    const std::vector<River>& rivers,
    const std::vector<int>& site_id_list) {
  std::vector<std::pair<int, int>> result;
  for (const auto& river : rivers) {
    int source = GetSiteIndex(site_id_list, river.source);
    int target = GetSiteIndex(site_id_list, river.target);
    result.emplace_back(source, target);
    result.emplace_back(target, source);
  }
  std::sort(result.begin(), result.end());
  return result;
}

}  // namespace

class Scorer::UnionFindSet {
 public:
  explicit UnionFindSet(size_t num_cells) : cells_(num_cells) {
  }

  void AddMine(const std::vector<int>& dist_map) {
    DCHECK_EQ(cells_.size(), dist_map.size());
    for (size_t site_index = 0; site_index < dist_map.size(); ++site_index) {
      auto& cell = cells_[site_index];
      cell.scores.push_back(dist_map[site_index] * dist_map[site_index]);
    }
  }

  void AddFuture(
      size_t mine_index, int mine_site_index, int target_site_index,
      int distance) {
    int score = distance * distance * distance;
    cells_[mine_site_index].scores[mine_index] = -score;
    cells_[target_site_index].scores[mine_index] = 2 * score;
  }

  int GetScore(int site_index, int mine_index) const {
    int index = FindIndex(site_index);
    return cells_[index].scores[mine_index];
  }

  bool IsConnected(int site_index1, int site_index2) const {
    return FindIndex(site_index1) == FindIndex(site_index2);
  }

  void Merge(int site_index1, int site_index2) {
    site_index1 = FindIndex(site_index1);
    site_index2 = FindIndex(site_index2);

    // If same set, do nothing.
    if (site_index1 == site_index2)
      return;

    // TODO use rank?
    auto& cell1 = cells_[site_index1];
    auto& cell2 = cells_[site_index2];
    for (size_t i = 0; i < cell1.scores.size(); ++i) {
      cell1.scores[i] += cell2.scores[i];
    }
    cell2.parent_index = site_index1;
  }
 private:
  struct Cell {
    mutable int parent_index = -1;  // -1 is root.
    std::vector<int> scores;  // Mine index -> score.
  };

  int FindIndex(int site_index) const {
    if (cells_[site_index].parent_index == -1)
      return site_index;
    // Short circuit.
    int root_index = FindIndex(cells_[site_index].parent_index);
    cells_[site_index].parent_index = root_index;
    return root_index;
  }

  std::vector<Cell> cells_;
};

Scorer::Scorer() = default;
Scorer::~Scorer() = default;

void Scorer::Initialize(const std::vector<PunterInfo>& punter_info_list,
                        const Map& game_map) {
  site_id_list_ = CreateSiteIdList(game_map.sites);
  mine_list_ = CreateMineList(game_map.sites, site_id_list_);

  for (size_t i = 0; i < punter_info_list.size(); ++i)
    sets_.emplace_back(site_id_list_.size());

  // Initialize UFSet instances.
  std::vector<std::pair<int, int>> edges =
      CreateBidirectionalGraph(game_map.rivers, site_id_list_);

  std::vector<int> dist_map(site_id_list_.size());
  for (int i = 0; i < mine_list_.size(); ++i) {
    int mine_site_index = mine_list_[i];

    // Initialize as unreached.
    std::fill(dist_map.begin(), dist_map.end(), -1);
    dist_map[mine_site_index] = 0;

    std::queue<int> q;
    q.push(mine_site_index);
    while (!q.empty()) {
      int site_index = q.front();
      q.pop();
      int dist = dist_map[site_index];
      DCHECK_GE(dist, 0);

      for (auto range = std::equal_range(
               edges.begin(), edges.end(),
               std::make_pair(site_index, 0),
               [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                 return a.first < b.first;
               });
           range.first != range.second; ++range.first) {
        int next_site_index = range.first->second;
        int& next_dist = dist_map[next_site_index];
        if (next_dist >= 0)
          continue;
        next_dist = dist + 1;
        q.push(next_site_index);
      }
    }

    const int mine_site_id = site_id_list_[mine_site_index];
    for (size_t punter_id = 0; punter_id < sets_.size(); ++punter_id) {
      auto& ufset = sets_[punter_id];
      ufset.AddMine(dist_map);

      // If future is set, pre calculate it. Only last one is valid.
      int target_site_id = -1;
      for (const auto& future : punter_info_list[punter_id].futures) {
        if (future.source == mine_site_id)
          target_site_id = future.target;
      }

      if (target_site_id == -1)
        continue;

      int target_site_index = GetSiteIndex(site_id_list_, target_site_id);
      ufset.AddFuture(
          i, mine_site_index, target_site_index, dist_map[target_site_index]);
    }
  }
}

int Scorer::GetScore(size_t punter_id) const {
  auto& ufset = sets_[punter_id];

  int score = 0;
  for (size_t i = 0; i < mine_list_.size(); ++i)
    score += ufset.GetScore(mine_list_[i], i);

  return score;
}

void Scorer::Claim(size_t punter_id, int site_id1, int site_id2) {
  auto& ufset = sets_[punter_id];
  int site_index1 = GetSiteIndex(site_id_list_, site_id1);
  int site_index2 = GetSiteIndex(site_id_list_, site_id2);
  ufset.Merge(site_index1, site_index2);
}

}  // namespace stadium