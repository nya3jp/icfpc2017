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

void InitializeSubprocess(Popen* subprocess) {
  base::SetNonBlocking(fileno(subprocess->stdout_read()));
}

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {
  if (!FLAGS_persistent)
    return;
  subprocess_ = base::MakeUnique<Popen>(shell_ + " --persistent");
  InitializeSubprocess(subprocess_.get());
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
  request->Set("map", Map::ToJson(*map));

  auto settings_value = base::MakeUnique<base::DictionaryValue>();
  bool empty_settings = true;
  if (settings.futures) {
    settings_value->SetBoolean("futures", true);
    empty_settings = false;
  }
  if (settings.splurges) {
    settings_value->SetBoolean("splurges", true);
    empty_settings = false;
  }
  if (!empty_settings)
    request->Set("settings", std::move(settings_value));

  std::string name;
  std::unique_ptr<base::DictionaryValue> response;
  if (FLAGS_persistent) {
    response = base::DictionaryValue::From(
        RunProcess(subprocess_.get(), *request, &name, base::TimeDelta::FromSeconds(10)));
  } else {
    Popen subprocess(shell_);
    InitializeSubprocess(&subprocess);

    response = base::DictionaryValue::From(
        RunProcess(&subprocess, *request, &name, base::TimeDelta::FromSeconds(10)));
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
          case Move::Type::SPLURGE:
            {
              auto splurge_dict = base::MakeUnique<base::DictionaryValue>();
              splurge_dict->SetInteger("punter", move.punter_id);
              splurge_dict->Set("route", common::ToJson(move.route));
              move_dict->Set("splurge", std::move(splurge_dict));
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
    response = base::DictionaryValue::From(
        RunProcess(subprocess_.get(), *request, nullptr, base::TimeDelta::FromSeconds(1)));
  } else {
    Popen subprocess(shell_);
    InitializeSubprocess(&subprocess);

    response = base::DictionaryValue::From(
        RunProcess(&subprocess, *request, nullptr, base::TimeDelta::FromSeconds(1)));
  }
  if (!response) {
    LOG(INFO) << "LOG: P" << punter_id_ << " timeout";
    return Move::Pass(punter_id_);
  }

  CHECK(response->Remove("state", &state_));
  return Move::FromJson(*response);
}

std::unique_ptr<base::Value> LocalPunter::RunProcess(
    Popen* subprocess,
    const base::DictionaryValue& request,
    std::string* out_name,
    const base::TimeDelta& timeout) {
  // Exchange names.
  base::Optional<std::string> name =
      common::ReadPing(subprocess->stdout_read());
  CHECK(name) << "Invalid greeting message.";
  if (out_name) {
    *out_name = name.value();
  }
  common::WritePong(subprocess->stdin_write(), name.value());

  // Start timer for timeout.
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Exchange the message.
  common::WriteMessage(subprocess->stdin_write(), request);
  std::unique_ptr<base::Value> result =
      common::ReadMessage(subprocess->stdout_read(), timeout, start_time);

  VLOG(3) << "Finished in " << (base::TimeTicks::Now() - start_time).InMilliseconds() << " ms";
  return result;
}

}  // namespace stadium
