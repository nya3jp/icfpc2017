#ifndef STADIUM_MASTER_H_
#define STADIUM_MASTER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "stadium/game_states.h"
#include "stadium/punter.h"
#include "stadium/referee.h"

namespace stadium {

class Master {
 public:
  Master();
  ~Master();

  void AddPunter(std::unique_ptr<Punter> punter);
  void RunGame(const Map& map);

 private:
  void Initialize(const Map& map);

  std::unique_ptr<Referee> referee_;
  std::vector<std::unique_ptr<Punter>> punters_;
  std::vector<Move> last_moves_;

  DISALLOW_COPY_AND_ASSIGN(Master);
};

}  // namespace stadium

#endif  // STADIUM_MASTER_H_
