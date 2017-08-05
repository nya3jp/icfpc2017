#include "punter/greedy_punter.h"

#include "base/memory/ptr_util.h"

namespace punter {

GreedyPunter::GreedyPunter() = default;
GreedyPunter::~GreedyPunter() = default;

void GreedyPunter::SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) {
  SimplePunter::SetUp(punter_id, num_punters, game_map);

  connected_from_mine_.resize(mines_.size());
  for (size_t i = 0; i < mines_.size(); i++) {
    connected_from_mine_[i].insert(mines_[i]);
  }
}

framework::GameMove GreedyPunter::Run() {
  const RiverWithPunter* river_with_max_score = nullptr;
  int max_score = -1;
  std::unique_ptr<std::set<std::pair<int, int>>> max_mines;  // {(mine_idx, site_id)}

  for (auto& r : rivers_) {
    if (r.punter != -1)
      continue;

    int score = 0;
    auto mines = base::MakeUnique<std::set<std::pair<int, int>>>();
    for (size_t mine_id = 0; mine_id < mines_.size(); mine_id++) {
      int target;
      if (connected_from_mine_[mine_id].count(r.source) && !connected_from_mine_[mine_id].count(r.target)) {
        target = r.target;
      } else if (!connected_from_mine_[mine_id].count(r.source) && connected_from_mine_[mine_id].count(r.target)) {
        target = r.source;
      } else {
        continue;
      }

      int dist = dist_to_mine_[site_id_to_site_idx_[target]][mine_id];
      score += dist * dist;
      mines->insert(std::make_pair(mine_id, target));
    }
    DLOG(INFO) << r.source << " -> " << r.target << ": " << score;
    if (score > max_score) {
      max_score = score;
      river_with_max_score = &r;
      max_mines = std::move(mines);
    }
  }

  if (river_with_max_score) {
    for (auto pair : *max_mines) {
      connected_from_mine_[pair.first].insert(pair.second);
    }
    return {framework::GameMove::Type::CLAIM, punter_id_,
        river_with_max_score->source, river_with_max_score->target};
  }
  return {framework::GameMove::Type::PASS, punter_id_};
}

void GreedyPunter::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));
  base::ListValue* value;
  CHECK(state->GetList("greedy", &value));

  connected_from_mine_.resize(value->GetSize());
  for (size_t mine_id = 0; mine_id < connected_from_mine_.size(); mine_id++) {
    base::ListValue* v;
    CHECK(value->GetList(mine_id, &v));
    for (size_t i = 0; i < v->GetSize(); i++) {
      int site;
      CHECK(v->GetInteger(i, &site));
      connected_from_mine_[mine_id].insert(site);
    }
  }

  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> GreedyPunter::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());

  auto value = base::MakeUnique<base::ListValue>();
  value->Reserve(mines_.size());
  for (size_t mine_id = 0; mine_id < mines_.size(); mine_id++) {
    auto v = base::MakeUnique<base::ListValue>();
    v->Reserve(connected_from_mine_[mine_id].size());
    for (int site : connected_from_mine_[mine_id]) {
      v->AppendInteger(site);
    }
    value->Append(std::move(v));
  }

  state->Set("greedy", std::move(value));
  return state;
}

} // namespace punter
