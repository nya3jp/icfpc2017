#include "stadium/local_punter.h"

#include <stdio.h>

#include <memory>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/files/scoped_file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "common/protocol.h"

DEFINE_bool(persistent, false, "Do not kill child process for each turn.");

namespace stadium {

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {
  if (!FLAGS_persistent)
    return;
  subprocess_ = base::MakeUnique<Popen>(shell_ + " --persistent");
  int fd = fileno(subprocess_->stdout_read());
  base::SetNonBlocking(fd);
}

LocalPunter::~LocalPunter() = default;

PunterInfo LocalPunter::Setup(int punter_id,
                              int num_punters,
                              const Map* map,
                              const Settings& settings) {
  punter_id_ = punter_id;

  auto request = base::MakeUnique<base::DictionaryValue>();
  request->SetInteger("punter", punter_id);
  request->SetInteger("punters", num_punters);
  request->Set("map", map->raw_value->CreateDeepCopy());
  if (settings.futures) {
    auto settings_value = base::MakeUnique<base::DictionaryValue>();
    settings_value->SetBoolean("futures", true);
    request->Set("settings", std::move(settings_value));
  }
  std::string name;
  std::unique_ptr<base::DictionaryValue> response;
  if (FLAGS_persistent) {
    response = base::DictionaryValue::From(RunProcess(subprocess_.get(), *request, &name, base::TimeDelta::FromSeconds(10)));
  } else {
    Popen subprocess(shell_);

    int fd = fileno(subprocess.stdout_read());
    base::SetNonBlocking(fd);

    response = base::DictionaryValue::From(RunProcess(&subprocess, *request, &name, base::TimeDelta::FromSeconds(10)));
  }
  CHECK(response) << "Setup() failed for punter " << punter_id_;
  CHECK(response->Remove("state", &state_));

  std::vector<River> futures;
  if (settings.futures) {
    const base::ListValue* futures_list;
    CHECK(response->GetList("futures", &futures_list));
    for (int i = 0; i < futures_list->GetSize(); ++i) {
      const base::DictionaryValue* future_value;
      CHECK(futures_list->GetDictionary(i, &future_value));
      int source, target;
      CHECK(future_value->GetInteger("source", &source));
      CHECK(future_value->GetInteger("target", &target));
      futures.push_back(River{source, target});
      // TDOO check if source is mine.
    }
  }
  return {name, futures};
}

Move LocalPunter::OnTurn(const std::vector<Move>& moves) {
  auto request = base::MakeUnique<base::DictionaryValue>();
  {
    auto action_dict = base::MakeUnique<base::DictionaryValue>();
    {
      auto moves_list = base::MakeUnique<base::ListValue>();
      for (const Move& move : moves) {
        auto move_dict = base::MakeUnique<base::DictionaryValue>();
        switch (move.type) {
          case Move::Type::PASS:
            {
              auto pass_dict = base::MakeUnique<base::DictionaryValue>();
              pass_dict->SetInteger("punter", move.punter_id);
              move_dict->Set("pass", std::move(pass_dict));
            }
            break;
          case Move::Type::CLAIM:
            {
              auto claim_dict = base::MakeUnique<base::DictionaryValue>();
              claim_dict->SetInteger("punter", move.punter_id);
              claim_dict->SetInteger("source", move.source);
              claim_dict->SetInteger("target", move.target);
              move_dict->Set("claim", std::move(claim_dict));
            }
            break;
        }
        moves_list->Append(std::move(move_dict));
      }
      action_dict->Set("moves", std::move(moves_list));
    }
    request->Set("move", std::move(action_dict));
    request->Set("state", state_->CreateDeepCopy());
  }

  // TODO: Implement timeout.
  std::unique_ptr<base::DictionaryValue> response;
  if (FLAGS_persistent) {
    response = base::DictionaryValue::From(RunProcess(subprocess_.get(), *request, nullptr, base::TimeDelta::FromSeconds(1)));
  } else {
    Popen subprocess(shell_);

    int fd = fileno(subprocess.stdout_read());
    base::SetNonBlocking(fd);

    response = base::DictionaryValue::From(RunProcess(&subprocess, *request, nullptr, base::TimeDelta::FromSeconds(1)));
  }
  if (!response) {
    LOG(INFO) << "Punter " << punter_id_ << " timeout";
    return Move::MakePass(punter_id_);
  }

  CHECK(response->Remove("state", &state_));

  if (response->HasKey("pass")) {
    base::DictionaryValue* action_dict;
    CHECK(response->GetDictionary("pass", &action_dict));
    int punter_id;
    CHECK(action_dict->GetInteger("punter", &punter_id));
    return Move::MakePass(punter_id);
  } else if (response->HasKey("claim")) {
    base::DictionaryValue* action_dict;
    CHECK(response->GetDictionary("claim", &action_dict));
    int punter_id, source, target;
    CHECK(action_dict->GetInteger("punter", &punter_id));
    CHECK(action_dict->GetInteger("source", &source));
    CHECK(action_dict->GetInteger("target", &target));
    return Move::MakeClaim(punter_id, source, target);
  } else {
    std::string text;
    CHECK(base::JSONWriter::Write(*response, &text));
    LOG(FATAL) << "Broken response: " << text;
    return Move::MakePass(punter_id_);
  }
}

std::unique_ptr<base::Value> LocalPunter::RunProcess(
    Popen* subprocess,
    const base::DictionaryValue& request,
    std::string* name,
    const base::TimeDelta& timeout) {
  auto ping = base::DictionaryValue::From(
      common::ReadMessage(subprocess->stdout_read(), base::TimeDelta(), base::TimeTicks()));
  CHECK(ping) << "Invalid greeting message";

  std::string tmp_name;
  CHECK(ping->GetString("me", &tmp_name)) << "Invalid greeting message";

  if (name) {
    *name = tmp_name;
  }

  auto pong = base::MakeUnique<base::DictionaryValue>();
  pong->SetString("you", tmp_name);
  base::TimeTicks start_time = base::TimeTicks::Now();
  common::WriteMessage(subprocess->stdin_write(), *pong);

  common::WriteMessage(subprocess->stdin_write(), request);
  return common::ReadMessage(subprocess->stdout_read(), timeout, start_time);
}

}  // namespace stadium
