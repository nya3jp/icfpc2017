#include "stadium/scorer.h"

#include <algorithm>
#include <utility>
#include <queue>

#include "base/logging.h"
#include "common/scorer.pb.h"

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

std::vector<int> CreateMineList(const std::vector<int>& mines,
                                const std::vector<int>& site_id_list) {
  std::vector<int> result;
  for (const auto& mine : mines) {
    result.push_back(GetSiteIndex(site_id_list, mine));
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

class DistanceMap {
 public:
  explicit DistanceMap(common::DistanceMapProto* proto) : proto_(proto) {}

  void Initialize(const Map& game_map,
                  const std::vector<int>& site_id_list,
                  const std::vector<int>& mine_list) {
    std::vector<std::pair<int, int>> edges =
        CreateBidirectionalGraph(game_map.rivers, site_id_list);

    for (size_t i = 0; i < mine_list.size(); ++i) {
      int mine_site_index = mine_list[i];
      auto* dist_map = proto_->add_entries();

      // Initialize as unreached.
      dist_map->mutable_distance()->Resize(site_id_list.size(), -1);
      dist_map->set_distance(mine_site_index, 0);

      std::queue<int> q;
      q.push(mine_site_index);
      while (!q.empty()) {
        int site_index = q.front();
        q.pop();
        int dist = dist_map->distance(site_index);
        DCHECK_GE(dist, 0);

        for (auto range = std::equal_range(
                 edges.begin(), edges.end(),
                 std::make_pair(site_index, 0),
                 [](const std::pair<int, int>& a,
                    const std::pair<int, int>& b) {
                   return a.first < b.first;
                 });
             range.first != range.second; ++range.first) {
          int next_site_index = range.first->second;
          if (dist_map->distance(next_site_index) >= 0)
            continue;
          dist_map->set_distance(next_site_index, dist + 1);
          q.push(next_site_index);
        }
      }
    }
  }

  size_t mine_size() const {
    return proto_->entries_size();
  }
  int GetDistance(int mine_index, int site_index) const {
    return proto_->entries(mine_index).distance(site_index);
  }

 private:
  common::DistanceMapProto* proto_;
  DISALLOW_COPY_AND_ASSIGN(DistanceMap);
};

class UnionFindSet {
 public:
  UnionFindSet(common::ScorerUnionFindSetProto* proto)
      : proto_(proto) {}

  void Initialize(size_t num_sites) {
    proto_->mutable_cells()->Reserve(num_sites);
    for (size_t i = 0; i < num_sites; ++i)
      proto_->add_cells();
  }

  void SetDistanceMap(const DistanceMap& distance_map) {
    for (size_t i = 0; i < proto_->cells_size(); ++i) {
      for (size_t j = 0; j < distance_map.mine_size(); ++j) {
        int dist = distance_map.GetDistance(j, i);
        mutable_cells(i)->add_scores(dist * dist);
      }
    }
  }

  void AddFuture(
      size_t mine_index, int mine_site_index, int target_site_index,
      int distance) {
    int score = distance * distance * distance;
    DCHECK_EQ(0, cells(mine_site_index).scores(mine_index));
    mutable_cells(mine_site_index)->set_scores(mine_index, -score);
    mutable_cells(target_site_index)->set_scores(
        mine_index, cells(target_site_index).scores(mine_index) + 2 * score);
  }

  int GetScore(int site_index, int mine_index) const {
    int index = FindIndex(site_index);
    return cells(index).scores(mine_index);
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
    auto* cell1 = mutable_cells(site_index1);
    auto* cell2 = mutable_cells(site_index2);
    for (size_t i = 0; i < cell1->scores_size(); ++i) {
      cell1->set_scores(i, cell1->scores(i) + cell2->scores(i));
    }
    cell2->set_parent_index(site_index1);
  }
 private:
  int FindIndex(int site_index) const {
    if (!cells(site_index).has_parent_index())
      return site_index;

    // Short circuit.
    int root_index = FindIndex(cells(site_index).parent_index());

    // Use mutable value.
    proto_->mutable_cells(site_index)->set_parent_index(root_index);
    return root_index;
  }

  const common::ScoreCell& cells(size_t index) const {
    return proto_->cells(index);
  }

  common::ScoreCell* mutable_cells(size_t index) {
    return proto_->mutable_cells(index);
  }

  mutable common::ScorerUnionFindSetProto* proto_;
  DISALLOW_COPY_AND_ASSIGN(UnionFindSet);
};

}  // namespace

Scorer::Scorer() = default;
Scorer::~Scorer() = default;

void Scorer::Initialize(size_t num_punters, const Map& game_map) {
  site_id_list_ = CreateSiteIdList(game_map.sites);
  mine_list_ = CreateMineList(game_map.mines, site_id_list_);

  DistanceMap distance_map(data_.mutable_distance_map());
  distance_map.Initialize(game_map, site_id_list_, mine_list_);

  for (size_t i = 0; i < num_punters; ++i) {
    UnionFindSet union_find_set(data_.add_scores());
    union_find_set.Initialize(site_id_list_.size());
    union_find_set.SetDistanceMap(distance_map);
  }
}

void Scorer::AddFuture(
    size_t punter_id, const std::vector<common::Future>& futures) {
  UnionFindSet union_find_set(data_.mutable_scores(punter_id));
  DistanceMap distance_map(data_.mutable_distance_map());

  // Assume futures is valid.
  for (const auto& future : futures) {
    int source_index = GetSiteIndex(site_id_list_, future.source);
    int mine_index = GetSiteIndex(mine_list_, source_index);
    int target_index = GetSiteIndex(site_id_list_, future.target);

    union_find_set.AddFuture(
        mine_index, source_index, target_index,
        distance_map.GetDistance(mine_index, target_index));
  }
}

int Scorer::GetScore(size_t punter_id) const {
  UnionFindSet ufset(data_.mutable_scores(punter_id));

  int score = 0;
  for (size_t i = 0; i < mine_list_.size(); ++i)
    score += ufset.GetScore(mine_list_[i], i);

  return score;
}

void Scorer::Claim(size_t punter_id, int site_id1, int site_id2) {
  UnionFindSet ufset(data_.mutable_scores(punter_id));
  int site_index1 = GetSiteIndex(site_id_list_, site_id1);
  int site_index2 = GetSiteIndex(site_id_list_, site_id2);
  ufset.Merge(site_index1, site_index2);
}

}  // namespace stadium
