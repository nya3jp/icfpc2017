#ifndef PUNTER_SIMULATING_PUNTER_H__
#define PUNTER_SIMULATING_PUNTER_H__

#include "framework/simple_punter.h"

#include <memory>
#include <vector>

namespace punter {

struct Snapshot;
struct Shadow;

class SimulatingPunter : public framework::SimplePunter {
 public:
  SimulatingPunter();
  ~SimulatingPunter() override;

  void SetUp(const common::SetUpData& args) override;
  framework::GameMove Run() override;

  std::unique_ptr<Shadow> SummonShadow() const;
  Snapshot GenerateSnapshot(const std::vector<framework::GameMove>& moves)
    const;
  void GenerateNextSnapshots(const Snapshot& old,
                             std::vector<Snapshot>* new_snapshots);
  void ShrinkToTop(std::vector<Snapshot>* snapshots);

 protected:
  common::SetUpData setup_data_;
  std::unique_ptr<SimplePunter> punter_;
  std::vector<framework::GameMove> old_moves_;
};

}  // namespace punter
#endif  // PUNTER_SIMULATING_PUNTER_H__
