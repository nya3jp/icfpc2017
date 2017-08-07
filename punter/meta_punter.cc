#include "punter/meta_punter.h"

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "common/protocol.h"
#include "gflags/gflags.h"


DEFINE_string(primary_worker, "", "Commandline for primary worker.");
DEFINE_string(backup_worker, "", "Commandline for backup worker");
DECLARE_bool(persistent);

namespace punter {

namespace {

constexpr base::TimeDelta kPrimaryTimeout =
    base::TimeDelta::FromMilliseconds(800);

void ExchangePingPong(common::Popen* subprocess) {
  base::Optional<std::string> name =
      common::ReadPing(subprocess->stdout_read());
  CHECK(name);
  common::WritePong(subprocess->stdin_write(), name.value());
}

}  // namespace

MetaPunter::MetaPunter() = default;

MetaPunter::~MetaPunter() = default;

void MetaPunter::OnInit() {
  primary_worker_ = base::MakeUnique<common::Popen>(
      FLAGS_primary_worker, true /* kill on parent death */);
  backup_worker_ = base::MakeUnique<common::Popen>(
      FLAGS_backup_worker, true /* kill on parent death */);
  base::SetNonBlocking(fileno(primary_worker_->stdout_read()));
  base::SetNonBlocking(fileno(backup_worker_->stdout_read()));
  ExchangePingPong(primary_worker_.get());
  ExchangePingPong(backup_worker_.get());
}

void MetaPunter::SetUp(const common::SetUpData& args) {
  auto request = common::SetUpData::ToJson(args);
  common::WriteMessage(primary_worker_->stdin_write(), *request);
  common::WriteMessage(backup_worker_->stdin_write(), *request);

  // TODO timeout.
  auto response1 = base::DictionaryValue::From(
      common::ReadMessage(primary_worker_->stdout_read()));
  auto response2 = base::DictionaryValue::From(
      common::ReadMessage(backup_worker_->stdout_read()));

  CHECK(response1->Remove("state", &primary_state_));
  CHECK(response2->Remove("state", &backup_state_));

  // If futures is returned by primary, then use it.
  base::ListValue* futures_value;
  if (response1->GetList("futures", &futures_value)) {
    futures_ = common::Futures::FromJson(*futures_value);
  }
}

std::vector<common::Future> MetaPunter::GetFutures() {
  return futures_;
}

common::GameMove MetaPunter::Run(
    const std::vector<common::GameMove>& moves) {
  const base::TimeTicks start = base::TimeTicks::Now();

  std::map<int, int> count;
  for (const auto& m : moves) {
    count[m.punter_id]++;
  }
  int max_count = 0;
  for (const auto& entry : count) {
    max_count = std::max(max_count, entry.second);
  }
  max_count = std::min(7, max_count);
  const base::TimeDelta timeout =
      kPrimaryTimeout
      - base::TimeDelta::FromMilliseconds((max_count - 1) * 100);
  DLOG(INFO) << "Timeout: " << timeout;

  // Run two workers in parallel.
  {
    base::DictionaryValue request;
    timeout_history_.insert(
        timeout_history_.end(), moves.begin(), moves.end());
    request.Set("move.moves", common::GameMoves::ToJson(timeout_history_));
    request.Set("state", primary_state_->CreateDeepCopy());
    common::WriteMessage(primary_worker_->stdin_write(), request);
  }
  {
    base::DictionaryValue request;
    request.Set("move.moves", common::GameMoves::ToJson(moves));
    request.Set("state", backup_state_->CreateDeepCopy());
    common::WriteMessage(backup_worker_->stdin_write(), request);
  }

  // Read backup worker first, which should be quickly done.
  // TODO: fix me.
  auto backup_response =
      base::DictionaryValue::From(
          common::ReadMessage(backup_worker_->stdout_read()));
  CHECK(backup_response->Remove("state", &backup_state_));

  auto primary_response =
      base::DictionaryValue::From(common::ReadMessage(
          primary_worker_->stdout_read(), timeout, start));

  if (!primary_response) {
    // TIMEOUT.
    return common::GameMove::FromJson(*backup_response);
  }

  CHECK(primary_response->Remove("state", &primary_state_));
  timeout_history_.clear();
  return common::GameMove::FromJson(*primary_response);
}

void MetaPunter::SetState(std::unique_ptr<base::Value> state_in) {
  // TODO merge map data.
  auto state = base::DictionaryValue::From(std::move(state_in));
  CHECK(state->Remove("primary", &primary_state_));
  CHECK(state->Remove("backup", &backup_state_));
  const base::ListValue* history_value;
  CHECK(state->GetList("history", &history_value));
  timeout_history_ = common::GameMoves::FromJson(*history_value);
}

std::unique_ptr<base::Value> MetaPunter::GetState() {
  auto state = base::MakeUnique<base::DictionaryValue>();
  state->Set(
      "primary",
      FLAGS_persistent ?
          primary_state_->CreateDeepCopy() :
          std::move(primary_state_));
  state->Set(
      "backup",
      FLAGS_persistent ?
          backup_state_->CreateDeepCopy() :
          std::move(backup_state_));
  state->Set("history", common::GameMoves::ToJson(timeout_history_));
  return state;
}

}  // namespace punter
