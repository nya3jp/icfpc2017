#ifndef PUNTER_SIMULATING_PUNTER_H__
#define PUNTER_SIMULATING_PUNTER_H__

#include "framework/simple_punter.h"

#include <memory>
#include <vector>

namespace punter {

struct Snapshot;

class SimulatingPunter : public framework::SimplePunter {
 public:
  SimulatingPunter();
  ~SimulatingPunter() override;

  void SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) override;
  framework::GameMove Run() override;
  // void SetState(std::unique_ptr<base::Value> state) override;
  // std::unique_ptr<base::Value> GetState() override;

  void ScoreFromMoves(const std::vector<framework::GameMove>& moves,
                      std::vector<int> *scores) const;
  Snapshot GenerateSnapshot(const std::vector<framework::GameMove>& moves)
    const;
  void GenerateNextSnapshots(const Snapshot& old,
                             std::vector<Snapshot>* new_snapshots);
  void ShrinkToTop(std::vector<Snapshot>* snapshots);

 protected:
  std::unique_ptr<SimplePunter> punter_;
  std::vector<framework::GameMove> old_moves_;
};

}  // namespace punter
#endif  // PUNTER_SIMULATING_PUNTER_H__
