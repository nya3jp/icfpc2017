#include "framework/game.h"

#include <stdio.h>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "gflags/gflags.h"

DEFINE_string(name, "", "Punter name.");

namespace framework {

namespace {

void SetStdinBlocking() {
  int flg = fcntl(0, F_GETFL);
  CHECK_GE(flg, 0);
  CHECK_EQ(0, fcntl(0, F_SETFL, flg & ~O_NONBLOCK));
}

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
  DLOG(INFO) << "size: " << size;
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
  fflush(file);
}

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
  framework::FromJson(*sites_value, &game_map.sites);
  framework::FromJson(*rivers_value, &game_map.rivers);
  framework::FromJson(*mines_value, &game_map.mines);
  return game_map;
}

std::unique_ptr<base::Value> GameMap::ToJson(const GameMap& game_map) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->Set("sites", framework::ToJson(game_map.sites));
  result->Set("rivers", framework::ToJson(game_map.rivers));
  result->Set("mines", framework::ToJson(game_map.mines));
  return result;
}

GameMove GameMove::Pass(int punter_id) {
  return {GameMove::Type::PASS, punter_id};
}

GameMove GameMove::Claim(int punter_id, int source, int target) {
  return {GameMove::Type::CLAIM, punter_id, source, target};
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

  LOG(FATAL) << "Unexpected key: " << value;
  return result;
}

std::unique_ptr<base::Value> GameMove::ToJson(const GameMove& game_move) {
  switch (game_move.type) {
    case GameMove::Type::CLAIM: {
      auto result = base::MakeUnique<base::DictionaryValue>();
      auto content = base::MakeUnique<base::DictionaryValue>();
      content->SetInteger("punter", game_move.punter_id);
      content->SetInteger("source", game_move.source);
      content->SetInteger("target", game_move.target);
      result->Set("claim", std::move(content));
      return result;
    }
    case GameMove::Type::PASS: {
      auto result = base::MakeUnique<base::DictionaryValue>();
      auto content = base::MakeUnique<base::DictionaryValue>();
      content->SetInteger("punter", game_move.punter_id);
      result->Set("pass", std::move(content));
      return result;
    }
  }
  LOG(FATAL) << "Unexpected type";
  return {};
}

Game::Game(std::unique_ptr<Punter> punter)
    : punter_(std::move(punter)) {}
Game::~Game() = default;

void Game::Run() {
  LOG(INFO) << "Set stdin blocking";
  SetStdinBlocking();

  DLOG(INFO) << "Game::Run";

  // Exchange name.
  DLOG(INFO) << "Exchanging name";
  {
    base::DictionaryValue name;
    name.SetString("me", FLAGS_name);
    DLOG(INFO) << "Sending name: " << name;
    WriteContent(stdout, name);
    DLOG(INFO) << "Reading name";
    auto input = base::DictionaryValue::From(ReadContent(stdin));
    DLOG(INFO) << "Read name: " << *input;
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

    GameMap game_map = GameMap::FromJson(*game_map_value);
    punter_->SetUp(punter_id, num_punters, game_map);

    base::DictionaryValue output;
    output.SetInteger("ready", punter_id);
    output.Set("state", punter_->GetState());
    WriteContent(stdout, output);
  } else if (input->HasKey("stop")) {
    // Game was over.
    const base::ListValue* moves_value;
    CHECK(input->GetList("stop.moves", &moves_value));
    std::vector<GameMove> moves;
    FromJson(*moves_value, &moves);
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
    std::vector<GameMove> moves;
    FromJson(*moves_value, &moves);

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
