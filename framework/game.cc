#include "framework/game.h"

#include <stdio.h>
#include <cctype>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "common/protocol.h"
#include "gflags/gflags.h"

DEFINE_string(name, "", "Punter name.");
DEFINE_bool(persistent, false, "If true, messages are repeatedly recieved");

namespace framework {

Game::Game(std::unique_ptr<Punter> punter)
    : punter_(std::move(punter)) {}
Game::~Game() = default;

void Game::Run() {
  do {
    if (RunImpl())
      break;
  } while (FLAGS_persistent);
}

bool Game::RunImpl() {
  DLOG(INFO) << "Game::Run";

  // Exchange name.
  DLOG(INFO) << "Exchanging name";
  {
    base::DictionaryValue name;
    name.SetString("me", FLAGS_name);
    DLOG(INFO) << "Sending name: " << name;
    common::WriteMessage(stdout, name);
    DLOG(INFO) << "Reading name";
    auto input = base::DictionaryValue::From(
        common::ReadMessage(stdin, base::TimeDelta(), base::TimeTicks()));
    DLOG(INFO) << "Read name: " << *input;
    std::string you_name;
    CHECK(input->GetString("you", &you_name));
    CHECK_EQ(FLAGS_name, you_name);
  }

  // Set up or play.
  auto input = base::DictionaryValue::From(
      common::ReadMessage(stdin, base::TimeDelta(), base::TimeTicks()));
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

    bool is_splurges = false;
    if (input->GetBoolean("settings.splurges", &is_splurges) &&
        is_splurges) {
      punter_->EnableSplurges();
    }

    base::DictionaryValue output;
    output.SetInteger("ready", punter_id);
    bool is_futures = false;
    if (input->GetBoolean("settings.futures", &is_futures) &&
        is_futures) {
      output.Set("futures", common::Futures::ToJson(punter_->GetFutures()));
    }
    if (FLAGS_persistent) {
      output.Set("state", base::MakeUnique<base::Value>());
    } else {
      output.Set("state", punter_->GetState());
    }
    common::WriteMessage(stdout, output);
  } else if (input->HasKey("stop")) {
    // Game was over.
#if DCHECK_IS_ON()
    const base::ListValue* moves_value;
    CHECK(input->GetList("stop.moves", &moves_value));
    std::vector<GameMove> moves = common::GameMoves::FromJson(*moves_value);
    for (const auto& m : moves) {
      switch (m.type) {
        case GameMove::Type::CLAIM:
          DLOG(INFO) << "move(claim): " << m.punter_id << ", "
                     << m.source << ", " << m.target;
          break;
        case GameMove::Type::PASS:
          DLOG(INFO) << "move(pass): " << m.punter_id;
          break;
        case GameMove::Type::SPLURGE:
          DLOG(INFO) << "move(splurge): " << m.punter_id;
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
      DLOG(INFO) << "score: " << punter_id << ", " << score;
    }
#endif
    return true;
  } else {
    // Play.
    const base::ListValue* moves_value;
    CHECK(input->GetList("move.moves", &moves_value));
    std::vector<GameMove> moves = common::GameMoves::FromJson(*moves_value);

    if (!FLAGS_persistent) {
      std::unique_ptr<base::Value> state;
      CHECK(input->Remove("state", &state));
      punter_->SetState(std::move(state));
    }

    GameMove result = punter_->Run(moves);

    base::DictionaryValue output;
    if (result.type == GameMove::Type::CLAIM) {
      auto claim = base::MakeUnique<base::DictionaryValue>();
      claim->SetInteger("punter", result.punter_id);
      claim->SetInteger("source", result.source);
      claim->SetInteger("target", result.target);
      output.Set("claim", std::move(claim));
    } else if (result.type == GameMove::Type::PASS) {
      auto pass = base::MakeUnique<base::DictionaryValue>();
      pass->SetInteger("punter", result.punter_id);
      output.Set("pass", std::move(pass));
    } else {
      CHECK(result.type == GameMove::Type::SPLURGE);
      auto splurge = base::MakeUnique<base::DictionaryValue>();
      splurge->SetInteger("punter", result.punter_id);
      splurge->Set("route", common::ToJson(result.route));
      output.Set("splurge", std::move(splurge));
    }
    if (FLAGS_persistent) {
      output.Set("state", base::MakeUnique<base::Value>());
    } else {
      output.Set("state", punter_->GetState());
    }

    common::WriteMessage(stdout, output);
  }

  return false;
}

}  // framework
