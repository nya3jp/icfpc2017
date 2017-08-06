#include "stadium/game_data.h"

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"

namespace stadium {

Map ReadMapFromFileOrDie(const std::string& path) {
  std::string map_content;
  CHECK(base::ReadFileToString(base::FilePath(path), &map_content));
  auto map_dict = base::DictionaryValue::From(
      base::JSONReader::Read(map_content));
  CHECK(map_dict);
  return Map::FromJson(*map_dict);
}

}  // namespace stadium
