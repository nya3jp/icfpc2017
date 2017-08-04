#include "stadium/game_states.h"

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace stadium {
namespace {

Map ParseMap(const base::DictionaryValue* map_dict) {
  CHECK(map_dict);

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
      const base::DictionaryValue* rive_dict;
      CHECK(rivers_list->GetDictionary(i, &river_dict));
      int source, target;
      CHECK(site_dict->GetInteger("source", &source));
      CHECK(site_dict->GetInteger("target", &target));
      map.rivers.emplace_back(River(source, target));
    }
  }

  return map;
}

}  // namespace

// static
Map Map::ReadFromFileOrDie(const std::string& path) {
  std::string map_content;
  CHECK(base::ReadFileToString(base::FilePath(path), &map_content));
  auto map_dict = base::DictionaryValue::From(base::JSONReader::Read(map_content));
  CHECK(map_dict);
  return ParseMap(map_dict.get());
}

}  // namespace stadium
