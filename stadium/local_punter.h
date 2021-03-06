#ifndef STADIUM_LOCAL_PUNTER_H_
#define STADIUM_LOCAL_PUNTER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "base/time/time.h"
#include "common/popen.h"
#include "stadium/punter.h"

namespace stadium {

class LocalPunter : public Punter {
 public:
  explicit LocalPunter(const std::string& shell);
  ~LocalPunter() override;

  PunterInfo SetUp(const common::SetUpData& args) override;
  base::Optional<Move> OnTurn(const std::vector<Move>& moves) override;
  void OnStop(const std::vector<Move>& moves,
              const std::vector<int>& scores) override;

 private:
  std::unique_ptr<base::Value> RunProcess(
      common::Popen* subprocess,
      const base::Value& request,
      std::string* out_name,
      const base::TimeDelta& timeout,
      bool expect_reply=true);

  const std::string shell_;

  int punter_id_;
  std::unique_ptr<base::Value> state_;
  std::unique_ptr<common::Popen> subprocess_;

  DISALLOW_COPY_AND_ASSIGN(LocalPunter);
};

}  // namespace stadium

#endif  // STADIUM_LOCAL_PUNTER_H_
