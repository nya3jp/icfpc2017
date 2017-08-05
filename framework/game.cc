#include "framework/game.h"

#include <stdio.h>

#include "base/optional.h"
#include "base/memory/ptr_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

namespace framework {

namespace {

base::Optional<std::string> ReadContent(FILE* file) {
  std::string content;
  constexpr size_t kBufferSize = 1 << 16;
  std::unique_ptr<char[]> buf(new char[kBufferSize]);
  for (size_t len; (len = fread(buf.get(), 1, kBufferSize, file)) > 0; ) {
    content.append(buf.get(), len);
  }
  if (ferror(file))
    return {};
  return content;
}

void WriteContent(FILE* file, const std::string& content) {
  size_t current = 0;
  while (current < content.size()) {
    size_t len =
        fwrite(content.c_str() + current, 1, content.size() - current, file);
    CHECK_GE(len, 0);
    current += len;
  }
}

std::vector<Site> ParseSites(const base::ListValue& value) {
  std::vector<Site> result;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    const base::DictionaryValue* ptr;
    CHECK(value.GetDictionary(i, &ptr));
    int id;
    CHECK(ptr->GetInteger("id", &id));
    result.emplace_back(Site{id});
  }
  return result;
}

std::vector<River> ParseRivers(const base::ListValue& value) {
  std::vector<River> result;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    const base::DictionaryValue* ptr;
    CHECK(value.GetDictionary(i, &ptr));
    int source, target;
    CHECK(ptr->GetInteger("source", &source));
    CHECK(ptr->GetInteger("target", &target));
    result.emplace_back(River{source, target});
  }
  return result;
}

std::vector<int> ParseMines(const base::ListValue& value) {
  std::vector<int> result;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    int mine;
    CHECK(value.GetInteger(i, &mine));
    result.push_back(mine);
  }
  return result;
}

GameMap ParseGameMap(const base::DictionaryValue& value) {
  GameMap result;
  const base::ListValue* sites_value;
  CHECK(value.GetList("sites", &sites_value));
  result.sites = ParseSites(*sites_value);
  const base::ListValue* rivers_value;
  CHECK(value.GetList("rivers", &rivers_value));
  result.rivers = ParseRivers(*rivers_value);
  const base::ListValue* mines_value;
  CHECK(value.GetList("mines", &mines_value));
  result.mines = ParseMines(*mines_value);

  return std::move(result);
}

GameMove ParseMove(const base::DictionaryValue& value) {
  GameMove result;
  const base::DictionaryValue* params;
  if (value.GetDictionary("claim", &params)) {
    result.type = GameMove::Type::CLAIM;
    CHECK(params->GetInteger("punter", &result.punter_id));
    CHECK(params->GetInteger("source", &result.source));
    CHECK(params->GetInteger("target", &result.target));
  } else {
    CHECK(value.GetDictionary("pass", &params));
    result.type = GameMove::Type::PASS;
    CHECK(params->GetInteger("punter", &result.punter_id));
  }
  return result;
}

std::vector<GameMove> ParseMoves(const base::ListValue& value) {
  std::vector<GameMove> result;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    const base::DictionaryValue* ptr;
    CHECK(value.GetDictionary(i, &ptr));
    result.push_back(ParseMove(*ptr));
  }
  return result;
}

}  // namespace


GameMove GameMove::Pass(int punter_id) {
  return {GameMove::Type::PASS, punter_id};
}

GameMove GameMove::Claim(int punter_id, int source, int target) {
  return {GameMove::Type::CLAIM, punter_id, source, target};
}

Game::Game(std::unique_ptr<Punter> punter)
    : punter_(std::move(punter)) {}
Game::~Game() = default;

void Game::Run() {
  auto content = ReadContent(stdin);
  auto input = base::DictionaryValue::From(
      base::JSONReader::Read(content.value()));
  if (input->HasKey("punter")) {
    // Set up.
    int punter_id;
    CHECK(input->GetInteger("punter", &punter_id));

    int num_punters;
    CHECK(input->GetInteger("punters", &num_punters));
    const base::DictionaryValue* game_map_value;
    CHECK(input->GetDictionary("map", &game_map_value));
    GameMap game_map = ParseGameMap(*game_map_value);
    punter_->SetUp(punter_id, num_punters, game_map);

    base::DictionaryValue output;
    output.SetInteger("ready", punter_id);
    output.Set("state", punter_->GetState());
    std::string output_content;
    CHECK(base::JSONWriter::Write(output, &output_content));
    WriteContent(stdout, output_content);
  } else {
    // Run.
    const base::ListValue* moves_value;
    CHECK(input->GetList("move.moves", &moves_value));
    std::vector<GameMove> moves = ParseMoves(*moves_value);
    std::unique_ptr<base::Value> state;
    CHECK(input->Remove("state", &state));
    punter_->SetState(std::move(state));
    GameMove result = punter_->Run(moves);

    base::DictionaryValue output;
    if (result.type == GameMove::Type::CLAIM) {
      auto claim = base::MakeUnique<base::DictionaryValue>();
      claim->SetInteger("punter", result.punter_id);
      claim->SetInteger("source", result.source);
      claim->SetInteger("target", result.target);
      output.Set("claim", std::move(claim));
    } else {
      CHECK(result.type == GameMove::Type::PASS);
      auto pass = base::MakeUnique<base::DictionaryValue>();
      pass->SetInteger("punter", result.punter_id);
      output.Set("pass", std::move(pass));
    }
    output.Set("state", punter_->GetState());

    std::string output_content;
    CHECK(base::JSONWriter::Write(output, &output_content));
    WriteContent(stdout, output_content);
  }
}

}  // framework
