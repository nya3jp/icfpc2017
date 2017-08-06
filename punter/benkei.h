#ifndef PUNTER_BENKEI_H_
#define PUNTER_BENKEI_H_

#include "framework/simple_punter.h"
#include "punter/greedy_punter_mirac.h"
#include "punter/benkei.pb.h"
#include <vector>

namespace punter {

class Benkei : public punter::GreedyPunterMirac {
 public:
  Benkei();
  ~Benkei() override;

  void SetUp(const common::SetUpData& args) override;

  framework::GameMove Run() override;

  BenkeiProto* getBenkeiProto() {
    return proto_.MutableExtension(BenkeiProto::benkei_ext);
  }

  bool IsRiverClaimed(int river_ix);
  bool IsRiverClaimed(int source, int dest);

 protected:
  void frequentpaths(int origin, const std::vector<int>& target_sites, std::vector<int>* freqs);


};

} // namespace punter

#endif  // PUNTER_BENKEI_H_
