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
    common::WritePing(stdout, FLAGS_name);
    base::Optional<std::string> you_name = common::ReadPong(stdin);
    CHECK(you_name);
    CHECK_EQ(FLAGS_name, you_name.value());
  }

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

    std::unique_ptr<base::Value> futures;
    bool is_futures_enabled = false;
    if (input->GetBoolean("settings.futures", &is_futures_enabled) &&
        is_futures_enabled) {
      // Signal the punter that the futures feature is enabled and get the
      // futures to send.
      futures = common::Futures::ToJson(punter_->GetFutures());
    }

    bool is_splurges_enabled = false;
    if (input->GetBoolean("settings.splurges", &is_splurges_enabled) &&
        is_splurges_enabled) {
      // Signal the punter that the splurges feature is enabled.
      punter_->EnableSplurges();
    }

    base::DictionaryValue output;

    output.SetInteger("ready", punter_id);

    if (is_futures_enabled) {
      output.Set("futures", std::move(futures));
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

    std::unique_ptr<base::DictionaryValue> output = GameMove::ToJson(result);

    if (FLAGS_persistent) {
      output->Set("state", base::MakeUnique<base::Value>());
    } else {
      output->Set("state", punter_->GetState());
    }

    common::WriteMessage(stdout, *output);
  }

  return false;
}

}  // framework
