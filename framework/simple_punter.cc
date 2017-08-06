#include "framework/simple_punter.h"

#include <queue>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "common/scorer.h"

namespace framework {

SimplePunter::SimplePunter() = default;
SimplePunter::~SimplePunter() = default;

GameMove SimplePunter::Run(const std::vector<GameMove>& moves) {
  common::Scorer scorer(proto_.mutable_scorer());
  for (auto move : moves) {
    if (move.type == GameMove::Type::CLAIM) {
      // Must use original site ids.
      scorer.Claim(move.punter_id, move.source, move.target);
    }

    if (move.type != GameMove::Type::PASS) {
      move.source = FindSiteIdxFromSiteId(move.source);
      move.target = FindSiteIdxFromSiteId(move.target);
    }
    if (move.type == GameMove::Type::CLAIM) {
      // TODO: This is slow!
      for (size_t i = 0; i < rivers_.size(); ++i) {
        auto& r = rivers_[i];
        if ((r.source == move.source && r.target == move.target) ||
            (r.source == move.target && r.target == move.source)) {
          DCHECK(r.punter == -1);
          r.punter = move.punter_id;
          proto_.mutable_game_map()->mutable_rivers(i)->set_punter(
              move.punter_id);
        }
      }
    }
  }

  GameMove out_move = Run();
  if (out_move.type != GameMove::Type::PASS) {
    out_move.source = sites_[out_move.source].id;
    out_move.target = sites_[out_move.target].id;
  }
  return out_move;
}

void SimplePunter::SetUp(
    int punter_id, int num_punters, const GameMap& game_map) {
  punter_id_ = punter_id;
  num_punters_ = num_punters;
  sites_ = game_map.sites;
  rivers_.reserve(game_map.rivers.size());
  for (auto& r : game_map.rivers) {
    rivers_.push_back(r);
  }
  mines_ = game_map.mines;

  std::sort(sites_.begin(), sites_.end(),
            [](const Site& lhs, const Site& rhs) {
              return lhs.id < rhs.id;
            });
  for (size_t i = 0; i < rivers_.size(); ++i) {
    rivers_[i].source = FindSiteIdxFromSiteId(rivers_[i].source);
    rivers_[i].target = FindSiteIdxFromSiteId(rivers_[i].target);
  }
  for (size_t i = 0; i < mines_.size(); ++i) {
    mines_[i] = FindSiteIdxFromSiteId(mines_[i]);
  }

  GenerateAdjacencyList();
  ComputeDistanceToMine();

  SaveToProto();

  common::Scorer(proto_.mutable_scorer()).Initialize(num_punters, game_map);
}

int SimplePunter::FindSiteIdxFromSiteId(int id) const {
  auto it = std::lower_bound(sites_.begin(), sites_.end(), id,
      [](const Site& site, int val) {
        return site.id < val;
      });
  DCHECK(it != sites_.end() && it->id == id);
  return it - sites_.begin();
}

void SimplePunter::SaveToProto() {
  proto_.set_punter_id(punter_id_);
  proto_.set_num_punters(num_punters_);
  GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (size_t i = 0; i < sites_.size(); ++i) {
    const auto& site = sites_[i];
    SiteProto* site_proto = game_map_proto->add_sites();
    site_proto->set_id(site.id);
    for (size_t k = 0; k < mines_.size(); ++k) {
      site_proto->add_to_mine()->set_distance(dist_to_mine_[i][k]);
    }
  }
  for (auto& r : rivers_) {
    RiverProto* river_proto = game_map_proto->add_rivers();
    river_proto->set_source(r.source);
    river_proto->set_target(r.target);
    river_proto->set_punter(-1);
  }
  for (auto& m : mines_) {
    game_map_proto->add_mines()->set_site(m);
  }
}

void SimplePunter::GenerateAdjacencyList() {
  edges_.resize(sites_.size());
  for (size_t i = 0; i < rivers_.size(); ++i) {
    const RiverWithPunter& river = rivers_[i];
    int a = river.source;
    int b = river.target;
    edges_[a].push_back(Edge{b, (int)i});
    edges_[b].push_back(Edge{a, (int)i});
  }
}

void SimplePunter::ComputeDistanceToMine() {
  size_t num_sites = sites_.size();
  size_t num_mines = mines_.size();
  dist_to_mine_.resize(num_sites, std::vector<int>(num_mines, -1));

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = mines_[i];

    std::queue<std::pair<int, int>> q;  // {(site_idx, dist)}
    dist_to_mine_[mine][i] = 0;
    q.push(std::make_pair(mine, 0));
    while(q.size()) {
      int site = q.front().first;
      int dist = q.front().second;
      q.pop();
      for (Edge edge : edges_[site]) {
        int next_site = edge.site;
        if (dist_to_mine_[next_site][i] != -1) continue;
        dist_to_mine_[next_site][i] = dist + 1;
        q.push(std::make_pair(next_site, dist + 1));
      }
    }
    for (size_t k = 0; k < num_sites; ++k) {
      DLOG(INFO) << "distance to mine " << mines_[i] << " from "
                 << sites_[k].id << ": " << dist_to_mine_[k][i];
    }
  }
}

void SimplePunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));

  std::string b64_proto;
  CHECK(state->GetString("proto", &b64_proto));
  std::string serialized;
  CHECK(base::Base64Decode(b64_proto, &serialized));
  CHECK(proto_.ParseFromString(serialized));

  punter_id_ = proto_.punter_id();
  num_punters_ = proto_.num_punters();

  const GameMapProto& game_map_proto = proto_.game_map();
  const size_t sites_size = game_map_proto.sites_size();
  sites_.resize(sites_size);
  for (size_t i = 0; i < sites_size; ++i) {
    sites_[i].id = game_map_proto.sites(i).id();
  }

  const size_t rivers_size = game_map_proto.rivers_size();
  rivers_.resize(rivers_size);
  for (size_t i = 0; i < rivers_size; ++i) {
    auto& r = rivers_[i];
    const RiverProto& rp = game_map_proto.rivers(i);
    r.source = rp.source();
    r.target = rp.target();
    r.punter = rp.punter();
  }

  const size_t mines_size = game_map_proto.mines_size();
  mines_.resize(mines_size);
  for (size_t i = 0; i < mines_size; ++i) {
    mines_[i] = game_map_proto.mines(i).site();
  }

  dist_to_mine_.resize(sites_.size(), std::vector<int>(mines_.size()));
  for (size_t i = 0; i < sites_.size(); ++i) {
    for (size_t k = 0; k < mines_.size(); ++k) {
      dist_to_mine_[i][k] = game_map_proto.sites(i).to_mine(k).distance();
    }
  }

  GenerateAdjacencyList();
}

std::unique_ptr<base::Value> SimplePunter::GetState() {
  auto value = base::MakeUnique<base::DictionaryValue>();
  const std::string binary = proto_.SerializeAsString();
  std::string b64;
  base::Base64Encode(binary, &b64);
  value->SetString("proto", b64);

  return std::move(value);
}

std::vector<Future> SimplePunter::GetFutures() {
  // Conver to original id before returning.
  std::vector<Future> result = GetFuturesImpl();
  for (auto& future : result) {
    future.source = sites_[future.source].id;
    future.target = sites_[future.target].id;
  }

  // Update future.
  common::Scorer(proto_.mutable_scorer()).AddFuture(punter_id_, result);

  return std::move(result);
}

int SimplePunter::GetScore(int punter_id) const {
  return common::Scorer(proto_.mutable_scorer()).GetScore(punter_id);
}

int SimplePunter::TryClaim(
    int punter_id, int site_index1, int site_index2) const {
  return common::Scorer(proto_.mutable_scorer()).TryClaim(
      punter_id, sites_[site_index1].id, sites_[site_index2].id);
}

bool SimplePunter::IsConnected(
    int punter_id, int site_index1, int site_index2) const {
  return common::Scorer(proto_.mutable_scorer()).IsConnected(
      punter_id, sites_[site_index1].id, sites_[site_index2].id);
}

std::vector<int> SimplePunter::GetConnectedMineList(
    int punter_id, int site_index) const {
  std::vector<int> result =
      common::Scorer(proto_.mutable_scorer()).GetConnectedMineList(
          punter_id, sites_[site_index].id);
  for (auto& site : result) {
    site = FindSiteIdxFromSiteId(site);
  }
  return result;
}

}  // namespace framework
