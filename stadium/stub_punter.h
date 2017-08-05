#ifndef STADIUM_STUB_PUNTER_H_
#define STADIUM_STUB_PUNTER_H_

#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "stadium/punter.h"

namespace stadium {

class StubPunter : public Punter {
 public:
  StubPunter();
  ~StubPunter() override;

  PunterInfo Setup(int punter_id,
                   int num_punters,
                   const Map* map,
                   const Settings& settings) override;
  Move OnTurn(const std::vector<Move>& moves) override;

 private:
  int punter_id_;
  std::set<std::pair<int, int>> unclaimed_rivers_;

  DISALLOW_COPY_AND_ASSIGN(StubPunter);
};

}  // namespace stadium

#endif  // STADIUM_STUB_PUNTER_H_
