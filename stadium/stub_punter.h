#ifndef STADIUM_STUB_PUNTER_H_
#define STADIUM_STUB_PUNTER_H_

#include <string>

#include "base/macros.h"
#include "stadium/punter.h"

namespace stadium {

class StubPunter : public Punter {
 public:
  StubPunter();
  ~StubPunter() override;

  std::string Setup(int punter_id,
                    int num_punters,
                    const Map* map) override;
  Move OnTurn(const std::vector<Move>& moves) override;

 private:
  int punter_id_;

  DISALLOW_COPY_AND_ASSIGN(StubPunter);
};

}  // namespace stadium

#endif  // STADIUM_STUB_PUNTER_H_
