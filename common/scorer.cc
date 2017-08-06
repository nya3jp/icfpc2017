#include "common/scorer.h"

#include <algorithm>
#include <utility>
#include <queue>

#include "base/logging.h"
#include "common/scorer.pb.h"

namespace common {

namespace {

void CreateSiteIdList(const std::vector<Site>& sites,
                      ::google::protobuf::RepeatedField<int>* output) {
  output->Reserve(sites.size());
  for (const auto& site : sites) {
    output->Add(site.id);
  }
  std::sort(output->begin(), output->end());
}

// site_id_list must be sorted array.
int GetIndex(const ::google::protobuf::RepeatedField<int>& sorted_container,
             int value) {
  auto it = std::lower_bound(
      sorted_container.begin(), sorted_container.end(), value);
  DCHECK_EQ(*it, value);
  return static_cast<int>(std::distance(sorted_container.begin(), it));
}

void CreateMineIndexList(
    const std::vector<int>& mines,
    const ::google::protobuf::RepeatedField<int>& site_id_list,
    ::google::protobuf::RepeatedField<int>* output) {
  output->Reserve(mines.size());
  for (int mine : mines) {
    output->Add(GetIndex(site_id_list, mine));
  }
  std::sort(output->begin(), output->end());
}

std::vector<std::pair<int, int>> CreateBidirectionalGraph(
    const std::vector<River>& rivers,
    const ::google::protobuf::RepeatedField<int>& site_id_list) {
  std::vector<std::pair<int, int>> result;
  for (const auto& river : rivers) {
    int source = GetIndex(site_id_list, river.source);
    int target = GetIndex(site_id_list, river.target);
    result.emplace_back(source, target);
    result.emplace_back(target, source);
  }
  std::sort(result.begin(), result.end());
  return result;
}

class DistanceMap {
 public:
  explicit DistanceMap(DistanceMapProto* proto) : proto_(proto) {}

  void Initialize(const GameMap& game_map,
                  const ::google::protobuf::RepeatedField<int>& site_id_list,
                  const ::google::protobuf::RepeatedField<int>& mine_list) {
    std::vector<std::pair<int, int>> edges =
        CreateBidirectionalGraph(game_map.rivers, site_id_list);

    for (int i = 0; i < mine_list.size(); ++i) {
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

  int mine_size() const {
    return proto_->entries_size();
  }
  int GetDistance(int mine_index, int site_index) const {
    return proto_->entries(mine_index).distance(site_index);
  }

 private:
  DistanceMapProto* proto_;
  DISALLOW_COPY_AND_ASSIGN(DistanceMap);
};

class UnionFindSet {
 public:
  UnionFindSet(ScorerUnionFindSetProto* proto)
      : proto_(proto) {}

  void Initialize(size_t num_sites) {
    proto_->mutable_cells()->Reserve(num_sites);
    for (size_t i = 0; i < num_sites; ++i)
      proto_->add_cells();
  }

  void SetDistanceMap(const DistanceMap& distance_map) {
    for (int i = 0; i < proto_->cells_size(); ++i) {
      for (int j = 0; j < distance_map.mine_size(); ++j) {
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
    for (int i = 0; i < cell1->scores_size(); ++i) {
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

  const ScoreCell& cells(size_t index) const {
    return proto_->cells(index);
  }

  ScoreCell* mutable_cells(size_t index) {
    return proto_->mutable_cells(index);
  }

  mutable ScorerUnionFindSetProto* proto_;
  DISALLOW_COPY_AND_ASSIGN(UnionFindSet);
};

}  // namespace

Scorer::Scorer(ScorerProto* data) : data_(data) {}
Scorer::~Scorer() = default;

void Scorer::Initialize(size_t num_punters, const GameMap& game_map) {
  CreateSiteIdList(game_map.sites, data_->mutable_site_ids());
  CreateMineIndexList(
      game_map.mines, data_->site_ids(), data_->mutable_mine_index_list());

  DistanceMap distance_map(data_->mutable_distance_map());
  distance_map.Initialize(
      game_map, data_->site_ids(), data_->mine_index_list());

  for (size_t i = 0; i < num_punters; ++i) {
    UnionFindSet union_find_set(data_->add_scores());
    union_find_set.Initialize(data_->site_ids_size());
    union_find_set.SetDistanceMap(distance_map);
  }
}

void Scorer::AddFuture(
    size_t punter_id, const std::vector<Future>& futures) {
  UnionFindSet union_find_set(data_->mutable_scores(punter_id));
  DistanceMap distance_map(data_->mutable_distance_map());

  // Assume futures is valid.
  for (const auto& future : futures) {
    int source_index = GetIndex(data_->site_ids(), future.source);
    int mine_index = GetIndex(data_->mine_index_list(), source_index);
    int target_index = GetIndex(data_->site_ids(), future.target);

    union_find_set.AddFuture(
        mine_index, source_index, target_index,
        distance_map.GetDistance(mine_index, target_index));
  }
}

int Scorer::GetScore(size_t punter_id) const {
  UnionFindSet ufset(data_->mutable_scores(punter_id));

  int score = 0;
  for (int i = 0; i < data_->mine_index_list_size(); ++i)
    score += ufset.GetScore(data_->mine_index_list(i), i);

  return score;
}

void Scorer::Claim(size_t punter_id, int site_id1, int site_id2) {
  UnionFindSet ufset(data_->mutable_scores(punter_id));
  int site_index1 = GetIndex(data_->site_ids(), site_id1);
  int site_index2 = GetIndex(data_->site_ids(), site_id2);
  ufset.Merge(site_index1, site_index2);
}

void Scorer::Splurge(size_t punter_id, const std::vector<int>& route) {
  for (int i = 0; i + 1U < route.size(); ++i)
    Claim(punter_id, route[i], route[i + 1]);
}

int Scorer::TryClaim(size_t punter_id, int site_id1, int site_id2) const {
  int score = GetScore(punter_id);
  UnionFindSet ufset(data_->mutable_scores(punter_id));

  int site_index1 = GetIndex(data_->site_ids(), site_id1);
  int site_index2 = GetIndex(data_->site_ids(), site_id2);

  if (ufset.IsConnected(site_index1, site_index2))
    return score;

  for (int i = 0; i < data_->mine_index_list_size(); ++i) {
    int mine_index = data_->mine_index_list(i);
    if (ufset.IsConnected(mine_index, site_index1)) {
      score += ufset.GetScore(site_index2, i);
      continue;
    }
    if (ufset.IsConnected(mine_index, site_index2)) {
      score += ufset.GetScore(site_index1, i);
      continue;
    }
  }

  return score;
}

bool Scorer::IsConnected(size_t punter_id, int site_id1, int site_id2) const {
  return UnionFindSet(data_->mutable_scores(punter_id)).IsConnected(
      GetIndex(data_->site_ids(), site_id1),
      GetIndex(data_->site_ids(), site_id2));
}

std::vector<int> Scorer::GetConnectedMineList(size_t punter_id, int site_id)
    const {
  std::vector<int> result;
  int site_index = GetIndex(data_->site_ids(), site_id);
  UnionFindSet ufset(data_->mutable_scores(punter_id));
  for (int i = 0; i < data_->mine_index_list_size(); ++i) {
    int mine_index = data_->mine_index_list(i);
    if (ufset.IsConnected(mine_index, site_index)) {
      result.push_back(data_->site_ids(mine_index));
    }
  }
  return result;
}


std::vector<int> Scorer::GetConnectedSiteList(size_t punter_id, int site_id)
    const {
  std::vector<int> result;
  int site_index = GetIndex(data_->site_ids(), site_id);
  UnionFindSet ufset(data_->mutable_scores(punter_id));
  for (int i = 0; i < data_->site_ids_size(); ++i) {
    if (ufset.IsConnected(i, site_index)) {
      result.push_back(data_->site_ids(i));
    }
  }
  return result;
}

}  // namespace stadium
