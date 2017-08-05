#include "stadium/local_punter.h"

#include <stdio.h>

#include <memory>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/posix/eintr_wrapper.h"

namespace stadium {

namespace {

std::pair<base::ScopedFD, base::ScopedFD> CreatePipe(int flags = 0) {
  int fds[2];
  PCHECK(pipe2(fds, flags) == 0);
  return {base::ScopedFD(fds[0]), base::ScopedFD(fds[1])};
}

class Popen {
 public:
  explicit Popen(const std::string& shell) {
    base::ScopedFD stdin_read, stdin_write;
    std::tie(stdin_read, stdin_write) = CreatePipe();
    base::ScopedFD stdout_read, stdout_write;
    std::tie(stdout_read, stdout_write) = CreatePipe();

    pid_t pid = fork();
    PCHECK(pid >= 0);
    if (pid == 0) {
      int max_fd = sysconf(_SC_OPEN_MAX);

      PCHECK(HANDLE_EINTR(dup2(stdin_read.get(), 0)) >= 0);
      stdin_read.reset();
      stdin_write.reset();

      PCHECK(HANDLE_EINTR(dup2(stdout_write.get(), 1)) >= 0);
      stdout_read.reset();
      stdout_write.reset();

      // Close all FDs other than stdin, stdout, stderr.
      for (int i = 3; i < max_fd; ++i) {
        close(i);
      }

      // Child.
      char* const commands [] = {
        const_cast<char*>("/bin/bash"),
        const_cast<char*>("-c"),
        const_cast<char*>(shell.c_str()),
        nullptr,
      };

      execv(commands[0], commands);
      PCHECK(false) << "EXEC is failed.";
    }

    // Parent
    stdin_read.reset();
    stdout_write.reset();

    pid_ = pid;
    stdin_write_.reset(fdopen(stdin_write.release(), "w"));
    CHECK(stdin_write_);
    stdout_read_.reset(fdopen(stdout_read.release(), "r"));
    CHECK(stdout_read_);
  }

  ~Popen() { Wait(); }

  FILE* stdin_write() const { return stdin_write_.get(); }
  FILE* stdout_read() const { return stdout_read_.get(); }

  void Wait() {
    // Maybe we need to kill all decendants?
    kill(pid_, SIGKILL);
    int status = -1;
    CHECK_EQ(pid_, waitpid(pid_, &status, 0));
  }

 private:
  pid_t pid_;
  base::ScopedFILE stdin_write_;
  base::ScopedFILE stdout_read_;
};

void WriteMessage(const base::Value& value, FILE* fp) {
  std::string text;
  CHECK(base::JSONWriter::Write(value, &text));
  fprintf(fp, "%d:", static_cast<int>(text.size()));
  CHECK_EQ(text.size(), fwrite(text.c_str(), 1, text.size(), fp));
  fflush(fp);
}

std::unique_ptr<base::Value> ReadMessage(FILE* fp) {
  int size = -1;
  {
    char buf[16];
    for (int i = 0; i < 10; ++i) {
      size_t result = fread(&buf[i], sizeof(char), 1, fp);
      PCHECK(result == 1) << "fread";
      if (buf[i] == ':') {
        buf[i] = '\0';
        CHECK(base::StringToInt(buf, &size));
        break;
      }
    }
  }
  CHECK(size >= 0) << "Invalid JSON message header";

  std::vector<char> buf(size, 'x');
  CHECK_EQ(static_cast<size_t>(size),
           fread(buf.data(), sizeof(char), size, fp));
  return base::JSONReader::Read(base::StringPiece(buf.data(), buf.size()));
}

}  // namespace

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {}

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
  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(RunProcess(*request, &name));
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
  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(RunProcess(*request, nullptr));
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
    const base::DictionaryValue& request,
    std::string* name) {
  Popen process(shell_);

  auto ping = base::DictionaryValue::From(ReadMessage(process.stdout_read()));
  CHECK(ping) << "Invalid greeting message";

  std::string tmp_name;
  CHECK(ping->GetString("me", &tmp_name)) << "Invalid greeting message";

  if (name) {
    *name = tmp_name;
  }

  auto pong = base::MakeUnique<base::DictionaryValue>();
  pong->SetString("you", tmp_name);
  WriteMessage(*pong, process.stdin_write());

  WriteMessage(request, process.stdin_write());
  return ReadMessage(process.stdout_read());
}

}  // namespace stadium
