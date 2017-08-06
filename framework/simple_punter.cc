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
      for (int i = 0; i < rivers_->size(); ++i) {
        RiverProto* r = rivers_->Mutable(i);
        if ((r->source() == move.source && r->target() == move.target) ||
            (r->source() == move.target && r->target() == move.source)) {
          DCHECK(r->punter() == -1);
          r->set_punter(move.punter_id);
        }
      }
    }
  }

  GameMove out_move = Run();
  if (out_move.type != GameMove::Type::PASS) {
    out_move.source = sites_->Get(out_move.source).id();
    out_move.target = sites_->Get(out_move.target).id();
  }
  return out_move;
}

void SimplePunter::SetUp(
    int punter_id, int num_punters, const GameMap& game_map) {
  punter_id_ = punter_id;
  num_punters_ = num_punters;
  GameMapProto* game_map_proto = proto_.mutable_game_map();
  for (const Site& s : game_map.sites) {
    SiteProto* site_proto = game_map_proto->add_sites();
    site_proto->set_id(s.id);
    for (size_t i = 0; i < game_map.mines.size(); ++i) {
      site_proto->add_to_mine()->set_distance(-1);
    }
  }
  sites_ = proto_.mutable_game_map()->mutable_sites();
  for (const River& r : game_map.rivers) {
    RiverProto* river_proto = game_map_proto->add_rivers();
    river_proto->set_source(r.source);
    river_proto->set_target(r.target);
    river_proto->set_punter(-1);
  }
  rivers_ = proto_.mutable_game_map()->mutable_rivers();
  for (const int mine : game_map.mines) {
    MineProto* mine_proto = game_map_proto->add_mines();
    mine_proto->set_site(mine);
  }
  mines_ = proto_.mutable_game_map()->mutable_mines();

  std::sort(sites_->begin(), sites_->end(),
            [](const SiteProto& lhs, const SiteProto& rhs) {
              return lhs.id() < rhs.id();
            });
  for (int i = 0; i < rivers_->size(); ++i) {
    rivers_->Mutable(i)->set_source(FindSiteIdxFromSiteId(rivers_->Get(i).source()));
    rivers_->Mutable(i)->set_target(FindSiteIdxFromSiteId(rivers_->Get(i).target()));
  }
  for (int i = 0; i < mines_->size(); ++i) {
    mines_->Mutable(i)->set_site(FindSiteIdxFromSiteId(mines_->Get(i).site()));
  }

  GenerateAdjacencyList();
  ComputeDistanceToMine();

  SaveToProto();

  common::Scorer(proto_.mutable_scorer()).Initialize(num_punters, game_map);
}

int SimplePunter::FindSiteIdxFromSiteId(int id) const {
  auto it = std::lower_bound(sites_->begin(), sites_->end(), id,
      [](const SiteProto& site, int val) {
        return site.id() < val;
      });
  DCHECK(it != sites_->end() && it->id() == id);
  return it - sites_->begin();
}

void SimplePunter::SaveToProto() {
  proto_.set_punter_id(punter_id_);
  proto_.set_num_punters(num_punters_);
}

void SimplePunter::GenerateAdjacencyList() {
  edges_.resize(sites_->size());
  for (int i = 0; i < rivers_->size(); ++i) {
    const RiverProto& river = rivers_->Get(i);
    int a = river.source();
    int b = river.target();
    edges_[a].push_back(Edge{b, i});
    edges_[b].push_back(Edge{a, i});
  }
}

void SimplePunter::ComputeDistanceToMine() {
  size_t num_sites = sites_->size();
  size_t num_mines = mines_->size();

  for (size_t i = 0; i < num_mines; ++i) {
    int mine = mines_->Get(i).site();

    std::queue<std::pair<int, int>> q;  // {(site_idx, dist)}
    set_dist_to_mine(mine, i, 0);
    q.push(std::make_pair(mine, 0));
    while(q.size()) {
      int site = q.front().first;
      int dist = q.front().second;
      q.pop();
      for (Edge edge : edges_[site]) {
        int next_site = edge.site;
        if (dist_to_mine(next_site, i) != -1) continue;
        set_dist_to_mine(next_site, i, dist + 1);
        q.push(std::make_pair(next_site, dist + 1));
      }
    }
    for (size_t k = 0; k < num_sites; ++k) {
      DLOG(INFO) << "distance to mine " << mines_->Get(i).site() << " from "
                 << sites_->Get(k).id() << ": " << dist_to_mine(k, i);
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
  sites_ = proto_.mutable_game_map()->mutable_sites();
  rivers_ = proto_.mutable_game_map()->mutable_rivers();
  mines_ = proto_.mutable_game_map()->mutable_mines();

  punter_id_ = proto_.punter_id();
  num_punters_ = proto_.num_punters();

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
    future.source = sites_->Get(future.source).id();
    future.target = sites_->Get(future.target).id();
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
      punter_id, sites_->Get(site_index1).id(), sites_->Get(site_index2).id());
}

bool SimplePunter::IsConnected(
    int punter_id, int site_index1, int site_index2) const {
  return common::Scorer(proto_.mutable_scorer()).IsConnected(
      punter_id, sites_->Get(site_index1).id(), sites_->Get(site_index2).id());
}

std::vector<int> SimplePunter::GetConnectedMineList(
    int punter_id, int site_index) const {
  std::vector<int> result =
      common::Scorer(proto_.mutable_scorer()).GetConnectedMineList(
          punter_id, sites_->Get(site_index).id());
  for (auto& site : result) {
    site = FindSiteIdxFromSiteId(site);
  }
  return result;
}

std::vector<int> SimplePunter::GetConnectedSiteList(
    int punter_id, int site_index) const {
  std::vector<int> result =
      common::Scorer(proto_.mutable_scorer()).GetConnectedSiteList(
          punter_id, sites_->Get(site_index).id());
  for (auto& site : result) {
    site = FindSiteIdxFromSiteId(site);
  }
  return result;
}

bool SimplePunter::IsConnectable(
    int punter_id, int site_index1, int site_index2) const {
  std::vector<int> visited(edges_.size());
  std::queue<int> q;
  q.push(site_index1);
  while (!q.empty()) {
    int site_index = q.front();
    q.pop();
    for (const auto& edge : edges_[site_index]) {
      const RiverProto& river = rivers_->Get(edge.river);
      if (river.has_punter() && river.punter() != -1 &&
          river.punter() != punter_id) {
        // This river is already used. Skip it.
        continue;
      }
      if (visited[edge.site]) {
        // Already visited.
        continue;
      }
      if (edge.site == site_index2) {
        // Reached site_index2.
        return true;
      }
      visited[edge.site] = true;
      q.push(edge.site);
    }
  }
  return false;
}

std::vector<int> SimplePunter::Simulate(const std::vector<GameMove>& moves)
    const {
  std::vector<GameMove> orig_move;
  for (auto m : moves) {
    // TODO: support splurge.
    m.source = sites_->Get(m.source).id();
    m.target = sites_->Get(m.target).id();
    orig_move.push_back(m);
  }
  return common::Scorer(proto_.mutable_scorer()).Simulate(orig_move);
}

int SimplePunter::GetClaimingPunter(int site_index1, int site_index2) const {
  for (const auto& edge : edges_[site_index1]) {
    if (edge.site == site_index2) {
      return rivers_->Get(edge.river).punter();
    }
  }
  return -2;
}

}  // namespace framework
