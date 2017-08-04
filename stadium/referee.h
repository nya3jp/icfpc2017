#ifndef STADIUM_REFEREE_H_
#define STADIUM_REFEREE_H_

#include "base/macros.h"
#include "stadium/game_states.h"

namespace stadium {

class Referee {
 public:
  Referee();
  ~Referee();

  void Initialize(int num_punters, const Map& map);
  void OnMove(const Move& move);
  void Finish();

 private:
  DISALLOW_COPY_AND_ASSIGN(Referee);
};

}  // namespace stadium

#endif  // STADIUM_REFEREE_H_
