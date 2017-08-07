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
  proto_.set_num_remaining_turns(
      static_cast<int>(proto_.num_remaining_turns() - moves.size()));
  common::Scorer scorer(proto_.mutable_scorer());
  recent_updated_.clear();
  for (const auto& move : moves) {
    switch (move.type) {
      case GameMove::Type::PASS: {
        if (move.punter_id == punter_id_)
          proto_.set_pass_count(proto_.pass_count() + 1);
        break;
      }
      case GameMove::Type::CLAIM: {
        if (move.punter_id == punter_id_)
          proto_.set_pass_count(0);

        // Must use original site ids.
        scorer.Claim(move.punter_id, move.source, move.target);

        int source_idx = FindSiteIdxFromSiteId(move.source);
        int target_idx = FindSiteIdxFromSiteId(move.target);

        for (Edge& edge : edges_[source_idx]) {
          if (edge.site != target_idx)
            continue;
          RiverProto* r = rivers_->Mutable(edge.river);
          DCHECK(r->punter() == -1);
          r->set_punter(move.punter_id);
          recent_updated_.push_back(edge.river);
          break;
        }
        break;
      }
      case GameMove::Type::OPTION: {
        if (move.punter_id == punter_id_) {
          proto_.set_pass_count(0);
          proto_.set_options_remaining(proto_.options_remaining() - 1);
        }

        // Must use original site ids.
        scorer.Option(move.punter_id, move.source, move.target);

        int source_idx = FindSiteIdxFromSiteId(move.source);
        int target_idx = FindSiteIdxFromSiteId(move.target);

        bool done = false;
        for (Edge& edge : edges_[source_idx]) {
          if (edge.site != target_idx)
            continue;
          RiverProto* r = rivers_->Mutable(edge.river);
          DCHECK(r->punter() != -1);
          DCHECK(r->option_punter() == -1);
          r->set_option_punter(move.punter_id);
          recent_updated_.push_back(edge.river);
          done = true;
          break;
        }
        DCHECK(done);
        break;
      }
      case GameMove::Type::SPLURGE: {
        if (move.punter_id == punter_id_)
          proto_.set_pass_count(0);

        // Must use original site ids.
        scorer.Splurge(move.punter_id, move.route);

        for (size_t i = 0; i + 1U < move.route.size(); ++i) {
          int source_idx = FindSiteIdxFromSiteId(move.route[i]);
          int target_idx = FindSiteIdxFromSiteId(move.route[i + 1]);

          bool done = false;
          for (Edge& edge : edges_[source_idx]) {
            if (edge.site != target_idx)
              continue;
            RiverProto* r = rivers_->Mutable(edge.river);
            if (r->punter() == -1) {
              r->set_punter(move.punter_id);
            } else {
              DCHECK(r->option_punter() == -1);
              r->set_option_punter(move.punter_id);
            }
            done = true;
            recent_updated_.push_back(edge.river);
            break;
          }
          DCHECK(done);
        }
        break;
      }
    }
  }

  //LOG(INFO) << "GetOptionsRemaining(): " << GetOptionsRemaining();
  //LOG(INFO) << "GetNumSplurgableEdges(): " << GetNumSplurgableEdges();
  GameMove out_move = Run();

  // Translate site indexes to the original id.
  InternalGameMoveToExternal(&out_move);

  if (can_splurge_ && out_move.type == GameMove::Type::CLAIM) {
    if (out_move.source % 2) {
      std::vector<int> route = {out_move.source, out_move.target};
      return CreateSplurge(&route);
    }
  }
  return out_move;
}

void SimplePunter::SetAliasesToProto() {
  sites_ = proto_.mutable_game_map()->mutable_sites();
  rivers_ = proto_.mutable_game_map()->mutable_rivers();
  mines_ = proto_.mutable_game_map()->mutable_mines();
}

void SimplePunter::SetUp(const common::SetUpData& args) {
  punter_id_ = args.punter_id;
  num_punters_ = args.num_punters;

  GameMapProto* game_map_proto = proto_.mutable_game_map();

  const GameMap& game_map = args.game_map;
  for (const Site& s : game_map.sites) {
    SiteProto* site_proto = game_map_proto->add_sites();
    site_proto->set_id(s.id);
  }

  for (const River& r : game_map.rivers) {
    RiverProto* river_proto = game_map_proto->add_rivers();
    river_proto->set_source(r.source);
    river_proto->set_target(r.target);
    river_proto->set_punter(-1);
    river_proto->set_option_punter(-1);
  }

  for (const int mine : game_map.mines) {
    MineProto* mine_proto = game_map_proto->add_mines();
    mine_proto->set_site(mine);
  }

  SetAliasesToProto();

  std::sort(sites_->begin(), sites_->end(),
            [](const SiteProto& lhs, const SiteProto& rhs) {
              return lhs.id() < rhs.id();
            });
  for (int i = 0; i < rivers_->size(); ++i) {
    rivers_->Mutable(i)->set_source(
        FindSiteIdxFromSiteId(rivers_->Get(i).source()));
    rivers_->Mutable(i)->set_target(
        FindSiteIdxFromSiteId(rivers_->Get(i).target()));
  }
  for (int i = 0; i < mines_->size(); ++i) {
    mines_->Mutable(i)->set_site(
        FindSiteIdxFromSiteId(mines_->Get(i).site()));
  }

  GenerateAdjacencyList();

  proto_.set_punter_id(punter_id_);
  proto_.set_num_punters(num_punters_);

  // -1 since the initial play contains fake pass.
  proto_.set_pass_count(-1);

  proto_.set_options_remaining(game_map.mines.size());

  proto_.set_num_remaining_turns(rivers_->size());

  common::Scorer(proto_.mutable_scorer()).Initialize(
      num_punters_, args.game_map);
}

int SimplePunter::FindSiteIdxFromSiteId(int id) const {
  auto it = std::lower_bound(sites_->begin(), sites_->end(), id,
      [](const SiteProto& site, int val) {
        return site.id() < val;
      });
  DCHECK(it != sites_->end() && it->id() == id);
  return it - sites_->begin();
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

void SimplePunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));

  std::string b64_proto;
  CHECK(state->GetString("proto", &b64_proto));
  std::string serialized;
  CHECK(base::Base64Decode(b64_proto, &serialized));
  auto proto_in = base::MakeUnique<GameStateProto>();
  CHECK(proto_in->ParseFromString(serialized));
  SetStateFromProto(std::move(proto_in));
}

void SimplePunter::SetStateFromProto(std::unique_ptr<GameStateProto> state_in) {
  proto_ = *std::move(state_in);
  SetAliasesToProto();

  punter_id_ = proto_.punter_id();
  num_punters_ = proto_.num_punters();

  can_splurge_ = proto_.has_can_splurge() && proto_.can_splurge();
  can_option_ = proto_.has_can_option() && proto_.can_option();

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

void SimplePunter::EnableSplurges() {
  can_splurge_ = true;

  proto_.set_can_splurge(can_splurge_);
}

void SimplePunter::EnableOptions() {
  can_option_ = true;

  proto_.set_can_option(can_option_);
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

int SimplePunter::GetOptionsRemaining() const {
  if (!can_option_)
    return 0;
  return proto_.options_remaining();
}

int SimplePunter::GetNumSplurgableEdges() const {
  return proto_.pass_count() + 1;
}

GameMove SimplePunter::CreatePass() const {
  return std::move(GameMove::Pass(punter_id_));
}

GameMove SimplePunter::CreateClaim(int source_index, int target_index) const {
  return std::move(GameMove::Claim(punter_id_, source_index, target_index));
}

GameMove SimplePunter::CreateSplurge(std::vector<int>* route_in_index) const {
  return std::move(GameMove::Splurge(punter_id_, route_in_index));
}

GameMove SimplePunter::CreateOption(int source_index, int target_index) const {
  return std::move(GameMove::Option(punter_id_, source_index, target_index));
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

std::vector<int> SimplePunter::GetConnectableSiteTableFromSite(
    int punter_id, int start_site) const {
  std::vector<int> visited(num_sites());
  std::queue<int> q;
  q.push(start_site);
  visited[start_site] = 1;
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
      visited[edge.site] = true;
      q.push(edge.site);
    }
  }
  return visited;
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

int SimplePunter::dist_to_mine(int site, int mine) const {
  int mine_site_id = sites_->Get(proto_.game_map().mines(mine).site()).id();
  int site_id = sites_->Get(site).id();
  return common::Scorer(proto_.mutable_scorer()).GetDistanceToMine(
      mine_site_id, site_id);
}

int SimplePunter::GetClaimingPunter(int site_index1, int site_index2) const {
  for (const auto& edge : edges_[site_index1]) {
    if (edge.site == site_index2) {
      return rivers_->Get(edge.river).punter();
    }
  }
  return -2;
}

int SimplePunter::GetOptioningPunter(int site_index1, int site_index2) const {
  for (const auto& edge : edges_[site_index1]) {
    if (edge.site == site_index2) {
      return rivers_->Get(edge.river).option_punter();
    }
  }
  return -2;
}

void SimplePunter::InternalGameMoveToExternal(GameMove* m) const {
  switch (m->type) {
    case GameMove::Type::CLAIM: {
      m->source = sites_->Get(m->source).id();
      m->target = sites_->Get(m->target).id();
      break;
    }
    case GameMove::Type::OPTION: {
      m->source = sites_->Get(m->source).id();
      m->target = sites_->Get(m->target).id();
      break;
    }
    case GameMove::Type::SPLURGE: {
      for (size_t i = 0; i < m->route.size(); ++i) {
        m->route[i] = sites_->Get(m->route[i]).id();
      }
      break;
    }
    case GameMove::Type::PASS: {
      break;
    }
  }
}

}  // namespace framework
