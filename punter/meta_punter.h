#ifndef PUNTER_META_PUNTER_H_
#define PUNTER_META_PUNTER_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "common/popen.h"
#include "framework/game.h"

namespace punter {

class MetaPunter : public framework::Punter {
 public:
  MetaPunter();
  ~MetaPunter() override;

  void OnInit() override;
  void SetUp(const common::SetUpData& args) override;
  std::vector<common::Future> GetFutures() override;
  common::GameMove Run(const std::vector<common::GameMove>& moves) override;
  void OnFinish() override;
  void SetState(std::unique_ptr<base::Value> state) override;
  std::unique_ptr<base::Value> GetState() override;

 private:
  // Tmp futures.
  std::vector<common::Future> futures_;

  std::unique_ptr<common::Popen> primary_worker_;
  std::unique_ptr<common::Popen> backup_worker_;

  std::unique_ptr<base::Value> primary_state_;
  std::unique_ptr<base::Value> backup_state_;

  std::vector<common::GameMove> timeout_history_;

  DISALLOW_COPY_AND_ASSIGN(MetaPunter);
};

}  // namespace punter

#endif  // PUNTER_META_PUNTER_H_
