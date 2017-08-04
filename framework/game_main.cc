#include "base/memory/ptr_util.h"

#include "framework/game.h"
#include "framework/pass_punter.h"

using namespace framework;

int main() {
  Game game(base::MakeUnique<PassPunter>());
  game.Run();
}
