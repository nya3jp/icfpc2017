#include "stadium/game_data.h"

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"

namespace stadium {
namespace {

Map ParseMap(std::unique_ptr<base::DictionaryValue> map_dict) {
  Map map;

  std::set<int> mines;
  {
    const base::ListValue* mines_list;
    CHECK(map_dict->GetList("mines", &mines_list));
    for (int i = 0; i < mines_list->GetSize(); ++i) {
      int site_id;
      CHECK(mines_list->GetInteger(i, &site_id));
      mines.insert(site_id);
    }
  }

  {
    const base::ListValue* sites_list;
    CHECK(map_dict->GetList("sites", &sites_list));
    for (int i = 0; i < sites_list->GetSize(); ++i) {
      const base::DictionaryValue* site_dict;
      CHECK(sites_list->GetDictionary(i, &site_dict));
      int site_id;
      CHECK(site_dict->GetInteger("id", &site_id));
      map.sites.emplace_back(Site{site_id, mines.count(site_id) > 0});
    }
  }

  {
    const base::ListValue* rivers_list;
    CHECK(map_dict->GetList("rivers", &rivers_list));
    for (int i = 0; i < rivers_list->GetSize(); ++i) {
      const base::DictionaryValue* river_dict;
      CHECK(rivers_list->GetDictionary(i, &river_dict));
      int source, target;
      CHECK(river_dict->GetInteger("source", &source));
      CHECK(river_dict->GetInteger("target", &target));
      map.rivers.emplace_back(River{source, target});
    }
  }

  map.raw_value = std::move(map_dict);

  return map;
}

}  // namespace

// static
Map Map::ReadFromFileOrDie(const std::string& path) {
  std::string map_content;
  CHECK(base::ReadFileToString(base::FilePath(path), &map_content));
  auto map_dict = base::DictionaryValue::From(base::JSONReader::Read(map_content));
  CHECK(map_dict);
  return ParseMap(std::move(map_dict));
}

}  // namespace stadium
