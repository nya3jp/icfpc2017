#include "framework/simple_punter.h"

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

  for (auto& r : rivers_) {
    if (r.punter == -1) {
      return { GameMove::Type::CLAIM, punter_id_, r.source, r.target };
    }
  }

  return {GameMove::Type::PASS, punter_id_};
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

  return std::move(value);
}

}  // namespace framework
