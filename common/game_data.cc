#include "common/game_data.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace common {

namespace {

template<typename T>
void FromJson(const base::ListValue& values, std::vector<T>* output) {
  output->reserve(values.GetSize());
  for (size_t i = 0; i < values.GetSize(); ++i) {
    const base::Value* value = nullptr;
    CHECK(values.Get(i, &value));
    output->push_back(T::FromJson(*value));
  }
}

void FromJson(const base::ListValue& values, std::vector<int>* output) {
  output->reserve(values.GetSize());
  for (size_t i = 0; i < values.GetSize(); ++i) {
    int value;
    CHECK(values.GetInteger(i, &value));
    output->push_back(value);
  }
}

}  // namespace

template<typename T>
std::unique_ptr<base::Value> ToJson(const std::vector<T>& elements) {
  auto result = base::MakeUnique<base::ListValue>();
  result->Reserve(elements.size());
  for (const auto& element : elements) {
    result->Append(T::ToJson(element));
  }
  return result;
}

std::unique_ptr<base::Value> ToJson(const std::vector<int>& elements) {
  auto result = base::MakeUnique<base::ListValue>();
  result->Reserve(elements.size());
  for (int element : elements) {
    result->AppendInteger(element);
  }
  return result;
}

Site Site::FromJson(const base::Value& value_in) {
  const base::DictionaryValue* value;
  CHECK(value_in.GetAsDictionary(&value));
  Site result;
  CHECK(value->GetInteger("id", &result.id));
  return result;
}

std::unique_ptr<base::Value> Site::ToJson(const Site& site) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetInteger("id", site.id);
  return result;
}

River River::FromJson(const base::Value& value_in) {
  const base::DictionaryValue* value;
  CHECK(value_in.GetAsDictionary(&value));
  River result;
  CHECK(value->GetInteger("source", &result.source));
  CHECK(value->GetInteger("target", &result.target));
  return result;
}

std::unique_ptr<base::Value> River::ToJson(const River& river) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetInteger("source", river.source);
  result->SetInteger("target", river.target);
  return result;
}

GameMap GameMap::FromJson(const base::Value& value_in) {
  const base::DictionaryValue* value;
  CHECK(value_in.GetAsDictionary(&value));

  const base::ListValue* sites_value = nullptr;
  CHECK(value->GetList("sites", &sites_value));
  const base::ListValue* rivers_value = nullptr;
  CHECK(value->GetList("rivers", &rivers_value));
  const base::ListValue* mines_value = nullptr;
  CHECK(value->GetList("mines", &mines_value));

  GameMap game_map;
  common::FromJson(*sites_value, &game_map.sites);
  common::FromJson(*rivers_value, &game_map.rivers);
  common::FromJson(*mines_value, &game_map.mines);
  return game_map;
}

std::unique_ptr<base::Value> GameMap::ToJson(const GameMap& game_map) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->Set("sites", common::ToJson(game_map.sites));
  result->Set("rivers", common::ToJson(game_map.rivers));
  result->Set("mines", common::ToJson(game_map.mines));
  return result;
}

GameMove GameMove::Pass(int punter_id) {
  return {GameMove::Type::PASS, punter_id};
}

GameMove GameMove::Claim(int punter_id, int source, int target) {
  return {GameMove::Type::CLAIM, punter_id, source, target};
}

GameMove GameMove::Splurge(int punter_id, std::vector<int> route) {
  GameMove move{GameMove::Type::SPLURGE, punter_id};
  std::swap(move.route, route);
  return std::move(move);
}

GameMove GameMove::FromJson(const base::Value& value_in) {
  const base::DictionaryValue* value;
  CHECK(value_in.GetAsDictionary(&value));

  GameMove result;
  if (value->HasKey("claim")) {
    result.type = GameMove::Type::CLAIM;
    CHECK(value->GetInteger("claim.punter", &result.punter_id));
    CHECK(value->GetInteger("claim.source", &result.source));
    CHECK(value->GetInteger("claim.target", &result.target));
    return result;
  }
  if (value->HasKey("pass")) {
    result.type = GameMove::Type::PASS;
    CHECK(value->GetInteger("pass.punter", &result.punter_id));
    return result;
  }
  if (value->HasKey("splurge")) {
    result.type = GameMove::Type::SPLURGE;
    CHECK(value->GetInteger("splurge.punter", &result.punter_id));
    const base::ListValue* route = nullptr;
    CHECK(value->GetList("splurge.route", &route));
    common::FromJson(*route, &result.route);
    return result;
  }

  LOG(FATAL) << "Unexpected key: " << value;
  return result;
}

std::unique_ptr<base::DictionaryValue> GameMove::ToJson(const GameMove& game_move) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  switch (game_move.type) {
    case GameMove::Type::CLAIM: {
      auto content = base::MakeUnique<base::DictionaryValue>();
      content->SetInteger("punter", game_move.punter_id);
      content->SetInteger("source", game_move.source);
      content->SetInteger("target", game_move.target);
      result->Set("claim", std::move(content));
      return result;
    }
    case GameMove::Type::PASS: {
      auto content = base::MakeUnique<base::DictionaryValue>();
      content->SetInteger("punter", game_move.punter_id);
      result->Set("pass", std::move(content));
      return result;
    }
    case GameMove::Type::SPLURGE: {
      auto content = base::MakeUnique<base::DictionaryValue>();
      content->SetInteger("punter", game_move.punter_id);
      content->Set("route", common::ToJson(game_move.route));
      result->Set("splurge", std::move(content));
      return result;
    }
  }
  LOG(FATAL) << "Unexpected type";
  return {};
}

std::vector<GameMove> GameMoves::FromJson(const base::ListValue& value) {
  std::vector<GameMove> result;
  common::FromJson(value, &result);
  return result;
}

std::unique_ptr<base::Value> Futures::ToJson(
    const std::vector<Future>& futures) {
  return common::ToJson(futures);
}

}  // namespace common
