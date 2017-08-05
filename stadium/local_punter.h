#ifndef STADIUM_LOCAL_PUNTER_H_
#define STADIUM_LOCAL_PUNTER_H_

#include <string>

#include "base/macros.h"
#include "stadium/punter.h"

namespace stadium {

class LocalPunter : public Punter {
 public:
  explicit LocalPunter(const std::string& shell);
  ~LocalPunter() override;

  std::string Setup(int punter_id,
                    int num_punters,
                    const Map* map) override;
  Move OnTurn(const std::vector<Move>& moves) override;

 private:
  const std::string shell_;
  int punter_id_;

  DISALLOW_COPY_AND_ASSIGN(LocalPunter);
};

}  // namespace stadium

#endif  // STADIUM_LOCAL_PUNTER_H_
