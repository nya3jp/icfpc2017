#ifndef STADIUM_REMOTE_PUNTER_H_
#define STADIUM_REMOTE_PUNTER_H_

#include "base/macros.h"
#include "stadium/punter.h"

namespace stadium {

class RemotePunter : public Punter {
 public:
  explicit RemotePunter(int port);
  ~RemotePunter() override;

  PunterInfo SetUp(const common::SetUpData& args) override;
  Move OnTurn(const std::vector<Move>& moves) override;

 private:
  const int port_;
  int punter_id_;

  DISALLOW_COPY_AND_ASSIGN(RemotePunter);
};

}  // namespace stadium

#endif  // STADIUM_REMOTE_PUNTER_H_
