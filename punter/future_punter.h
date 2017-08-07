#ifndef PUNTER_FUTURE_PUNTER_H_
#define PUNTER_FUTURE_PUNTER_H_

#include "framework/simple_punter.h"

namespace punter {

class FuturePunter : public framework::SimplePunter {
 public:
  FuturePunter();
  ~FuturePunter() override;

  std::vector<framework::Future> GetFuturesImpl() override;

  framework::GameMove Run() override;
  framework::GameMove RunMirac();

 private:
  void GenerateRiversToClaim();
  framework::GameMove TryToConnect(int mine, int site);

  // site_idx -> mine_idx -> (how many rivers do we need to
  // claim in order to connect the site and the mine, site_idx of
  // previous location)
  std::vector<std::vector<std::pair<int, int>>> rivers_to_claim_;
};

} // namespace punter

#endif  // PUNTER_FUTURE_PUNTER_H_
