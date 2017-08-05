#include "framework/simple_punter.h"

#include <queue>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace framework {

SimplePunter::SimplePunter() = default;
SimplePunter::~SimplePunter() = default;

GameMove SimplePunter::Run(const std::vector<GameMove>& moves) {
  for (auto& move : moves) {
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
  return Run();
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

  GenerateSiteIdToSiteIndex();
  GenerateAdjacencyList();
  ComputeDistanceToMine();

  SaveToProto();
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
  for (const RiverWithPunter& river : rivers_) {
    int a = site_id_to_site_idx_[river.source];
    int b = site_id_to_site_idx_[river.target];
    edges_[a].push_back(b);
    edges_[b].push_back(a);
  }
  for (size_t i = 0; i < sites_.size(); ++i) {
    std::sort(edges_[i].begin(), edges_[i].end());
    edges_[i].erase(std::unique(edges_[i].begin(), edges_[i].end()),
                    edges_[i].end());
  }
}

void SimplePunter::GenerateSiteIdToSiteIndex() {
  for (size_t i = 0; i < sites_.size(); ++i) {
    site_id_to_site_idx_[sites_[i].id] = i;
  }
}

void SimplePunter::ComputeDistanceToMine() {
  size_t num_sites = sites_.size();
  size_t num_mines = mines_.size();
  dist_to_mine_.resize(num_sites, std::vector<int>(num_mines, -1));

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = site_id_to_site_idx_[mines_[i]];

    std::queue<std::pair<int, int>> q;  // {(site_idx, dist)}
    dist_to_mine_[mine][i] = 0;
    q.push(std::make_pair(mine, 0));
    while(q.size()) {
      int site = q.front().first;
      int dist = q.front().second;
      q.pop();
      for (int next : edges_[site]) {
        if (dist_to_mine_[next][i] != -1) continue;
        dist_to_mine_[next][i] = dist + 1;
        q.push(std::make_pair(next, dist + 1));
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

  GenerateSiteIdToSiteIndex();
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

}  // namespace framework
