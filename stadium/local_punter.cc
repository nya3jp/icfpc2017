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

void InitializeSubprocess(common::Popen* subprocess) {
  base::SetNonBlocking(fileno(subprocess->stdout_read()));
}

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {
  if (!FLAGS_persistent)
    return;
  subprocess_ = base::MakeUnique<common::Popen>(shell_ + " --persistent");
  InitializeSubprocess(subprocess_.get());
}

LocalPunter::~LocalPunter() = default;

PunterInfo LocalPunter::SetUp(const common::SetUpData& args) {
  punter_id_ = args.punter_id;

  auto request = common::SetUpData::ToJson(args);

  std::string name;
  std::unique_ptr<base::DictionaryValue> response;
  if (FLAGS_persistent) {
    response = base::DictionaryValue::From(
        RunProcess(subprocess_.get(), *request, &name, base::TimeDelta::FromSeconds(10)));
  } else {
    common::Popen subprocess(shell_);
    InitializeSubprocess(&subprocess);

    response = base::DictionaryValue::From(
        RunProcess(&subprocess, *request, &name, base::TimeDelta::FromSeconds(10)));
  }
  CHECK(response) << "Setup() failed for punter " << punter_id_;
  CHECK(response->Remove("state", &state_));

  std::vector<River> futures;
  if (args.settings.futures) {
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

base::Optional<Move> LocalPunter::OnTurn(const std::vector<Move>& moves) {
  auto request = base::MakeUnique<base::DictionaryValue>();
  {
    auto action_dict = base::MakeUnique<base::DictionaryValue>();
    action_dict->Set("moves", common::GameMoves::ToJson(moves));
    request->Set("move", std::move(action_dict));
    request->Set("state", state_->CreateDeepCopy());
  }

  // TODO: Implement timeout.
  std::unique_ptr<base::DictionaryValue> response;
  if (FLAGS_persistent) {
    response = base::DictionaryValue::From(
        RunProcess(subprocess_.get(), *request, nullptr, base::TimeDelta::FromSeconds(1)));
  } else {
    common::Popen subprocess(shell_);
    InitializeSubprocess(&subprocess);

    response = base::DictionaryValue::From(
        RunProcess(&subprocess, *request, nullptr, base::TimeDelta::FromSeconds(1)));
  }
  if (!response) {
    LOG(INFO) << "LOG: P" << punter_id_ << " timeout";
    return base::nullopt;
  }

  CHECK(response->Remove("state", &state_));
  return Move::FromJson(*response);
}

std::unique_ptr<base::Value> LocalPunter::RunProcess(
    common::Popen* subprocess,
    const base::Value& request,
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
