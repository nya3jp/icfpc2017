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

  punter_->OnInit();

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
    common::SetUpData args = common::SetUpData::FromJson(*input);
    punter_->SetUp(args);

    std::unique_ptr<base::Value> futures;
    if (args.settings.futures) {
      // Signal the punter that the futures feature is enabled and get the
      // futures to send.
      futures = common::Futures::ToJson(punter_->GetFutures());
    }

    if (args.settings.splurges) {
      // Signal the punter that the splurges feature is enabled.
      punter_->EnableSplurges();
    }

    base::DictionaryValue output;

    output.SetInteger("ready", args.punter_id);

    if (args.settings.futures) {
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
