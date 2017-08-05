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
      for (auto& r : rivers_) {
        if (r.source == move.source && r.target == move.target) {
          CHECK(r.punter == -1);
          r.punter = move.punter_id;
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

  CHECK(state->GetInteger("punter_id", &punter_id_));
  CHECK(state->GetInteger("num_punters", &num_punters_));

  base::ListValue* sites;
  CHECK(state->GetList("sites", &sites));
  sites_.resize(sites->GetSize());
  for (size_t i = 0; i < sites->GetSize(); i++) {
    CHECK(sites->GetInteger(i, &sites_[i].id));
  }

  base::ListValue* rivers;
  CHECK(state->GetList("rivers", &rivers));
  rivers_.resize(rivers->GetSize());
  for (size_t i = 0; i < rivers->GetSize(); i++) {
    base::DictionaryValue* river;
    CHECK(rivers->GetDictionary(i, &river));
    CHECK(river->GetInteger("source", &rivers_[i].source));
    CHECK(river->GetInteger("target", &rivers_[i].target));
    CHECK(river->GetInteger("punter", &rivers_[i].punter));
  }
  
  base::ListValue* mines;
  CHECK(state->GetList("mines", &mines));
  mines_.resize(mines->GetSize());
  for (size_t i = 0; i < mines->GetSize(); i++) {
    CHECK(mines->GetInteger(i, &mines_[i]));
  }

  base::ListValue* dist_to_mine;
  CHECK(state->GetList("dist_to_mine", &dist_to_mine));
  dist_to_mine_.resize(sites_.size(), std::vector<int>(mines_.size()));
  for (size_t i = 0; i < sites_.size(); ++i) {
    for (size_t k = 0; k < mines_.size(); ++k) {
      dist_to_mine->GetInteger(i * mines_.size() + k,
                               &dist_to_mine_[i][k]);
    }
  }
  std::string b64_proto;
  CHECK(state->GetString("proto", &b64_proto));
  std::string serialized;
  CHECK(base::Base64Decode(b64_proto, &serialized));
  CHECK(proto_.ParseFromString(serialized));

  GenerateSiteIdToSiteIndex();
  GenerateAdjacencyList();
}

std::unique_ptr<base::Value> SimplePunter::GetState() {
  auto value = base::MakeUnique<base::DictionaryValue>();
  value->SetInteger("punter_id", punter_id_);
  value->SetInteger("num_punters", num_punters_);
  
  auto sites = base::MakeUnique<base::ListValue>();
  sites->Reserve(sites_.size());
  for (auto& site : sites_) {
    sites->AppendInteger(site.id);
  }
  value->Set("sites", std::move(sites));

  auto rivers = base::MakeUnique<base::ListValue>();
  rivers->Reserve(rivers_.size());
  for (auto& river : rivers_) {
    auto v = base::MakeUnique<base::DictionaryValue>();
    v->SetInteger("source", river.source);
    v->SetInteger("target", river.target);
    v->SetInteger("punter", -1);
    rivers->Append(std::move(v));
  }
  value->Set("rivers", std::move(rivers));

  auto mines = base::MakeUnique<base::ListValue>();
  mines->Reserve(mines_.size());
  for (auto& mine : mines_) {
    mines->AppendInteger(mine);
  }
  value->Set("mines", std::move(mines));

  auto dist_to_mine = base::MakeUnique<base::ListValue>();
  dist_to_mine->Reserve(sites_.size() * mines_.size());
  for (size_t i = 0; i < sites_.size(); ++i) {
    for (size_t k = 0; k < mines_.size(); ++k) {
      dist_to_mine->AppendInteger(dist_to_mine_[i][k]);
    }
  }
  value->Set("dist_to_mine", std::move(dist_to_mine));

  const std::string binary = proto_.SerializeAsString();
  std::string b64;
  base::Base64Encode(binary, &b64);
  value->SetString("proto", b64);

  return std::move(value);
}

}  // namespace framework
