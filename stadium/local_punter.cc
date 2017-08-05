#include "stadium/local_punter.h"

#include <stdio.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"

namespace stadium {

namespace {

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
      CHECK_EQ(1, fread(&buf[i], sizeof(char), 1, fp));
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
  return base::JSONReader::Read(buf.data());
}

}  // namespace

LocalPunter::LocalPunter(const std::string& shell)
    : shell_(shell) {}

LocalPunter::~LocalPunter() = default;

std::string LocalPunter::Setup(int punter_id,
                               int num_punters,
                               const Map* map) {
  punter_id_ = punter_id;

  auto request = base::MakeUnique<base::DictionaryValue>();
  request->SetInteger("punter", punter_id);
  request->SetInteger("punters", num_punters);
  request->Set("map", map->raw_value->CreateDeepCopy());
  std::string name;
  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(RunProcess(*request, &name));
  CHECK(response->Remove("state", &state_));
  return name;
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
    CHECK(response->GetDictionary("cliam", &action_dict));
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
  base::ScopedFILE fp(popen(shell_.c_str(), "w"));
  CHECK(fp);

  std::unique_ptr<base::DictionaryValue> ping =
      base::DictionaryValue::From(ReadMessage(fp.get()));
  CHECK(ping) << "Invalid greeting message";

  std::string tmp_name;
  CHECK(ping->GetString("name", &tmp_name)) << "Invalid greeting message";

  if (name) {
    *name = tmp_name;
  }

  auto pong = base::MakeUnique<base::DictionaryValue>();
  pong->SetString("name", tmp_name);
  WriteMessage(*pong, fp.get());

  WriteMessage(request, fp.get());
  return ReadMessage(fp.get());
}

}  // namespace stadium
