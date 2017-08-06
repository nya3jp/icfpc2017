#ifndef STADIUM_GAME_H_
#define STADIUM_GAME_H_

#include <vector>

#include "base/macros.h"
#include "base/values.h"

#include "common/game_data.h"

namespace stadium {

using Site = common::Site;
using River = common::River;
using Map = common::GameMap;
using Move = common::GameMove;

Map ReadMapFromFileOrDie(const std::string& path);

}  // namespace stadium

#endif  // STADIUM_GAME_H_
