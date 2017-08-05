#include "framework/game.h"

#include <stdio.h>
#include <cctype>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "gflags/gflags.h"

DEFINE_string(name, "", "Punter name.");

namespace framework {

namespace {

int ReadLeadingInt(FILE* file) {
  char buf[10];
  for (size_t i = 0; ; ++i) {
    int c = fgetc(file);
    if (!std::isdigit(c)) {
      buf[i] = '\0';
      break;
    }
    buf[i] = static_cast<char>(c);
  }
  return std::atoi(buf);
}

std::unique_ptr<base::Value> ReadContent(FILE* file) {
  size_t size = ReadLeadingInt(file);
  std::unique_ptr<char[]> buf(new char[size]);
  size_t len = fread(buf.get(), 1, size, file);
  if (len != size)
    return nullptr;
  return base::JSONReader::Read(buf.get());
}

void WriteContent(FILE* file, const base::Value& content) {
  std::string output;
  CHECK(base::JSONWriter::Write(content, &output));
  std::string length = std::to_string(output.size());
  CHECK_EQ(length.size(), fwrite(length.c_str(), 1, length.size(), file));
  fputc(':', file);
  CHECK_EQ(output.size(), fwrite(output.c_str(), 1, output.size(), file));
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
  // Exchange name.
  {
    base::DictionaryValue name;
    name.SetString("me", FLAGS_name);
    WriteContent(stdout, name);
    auto input = base::DictionaryValue::From(ReadContent(stdin));
    std::string you_name;
    CHECK(input->GetString("you", &you_name));
    CHECK_EQ(FLAGS_name, you_name);
  }

  // Set up or play.
  auto input = base::DictionaryValue::From(ReadContent(stdin));
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
    WriteContent(stdout, output);
  } else if (input->HasKey("stop")) {
    // Game was over.
    const base::ListValue* moves_value;
    CHECK(input->GetList("stop.moves", &moves_value));
    std::vector<GameMove> moves = ParseMoves(*moves_value);
    for (const auto& m : moves) {
      switch (m.type) {
        case GameMove::Type::CLAIM:
          LOG(INFO) << "move(claim): " << m.punter_id << ", "
                    << m.source << ", " << m.target;
          break;
        case GameMove::Type::PASS:
          LOG(INFO) << "move(pass): " << m.punter_id;
          break;
      }
    }

    const base::ListValue* scores_value;
    CHECK(input->GetList("stop.scores", &scores_value));
    for (size_t i = 0; i < scores_value->GetSize(); ++i) {
      const base::DictionaryValue* score_value;
      CHECK(scores_value->GetDictionary(i, &score_value));
      int punter_id;
      CHECK(score_value->GetInteger("punter", &punter_id));
      int score;
      CHECK(score_value->GetInteger("score", &score));
      LOG(INFO) << "score: " << punter_id << ", " << score;
    }
  } else {
    // Play.
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

    WriteContent(stdout, output);
  }
}

}  // framework
