#ifndef STADIUM_LOCAL_PUNTER_H_
#define STADIUM_LOCAL_PUNTER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "base/time/time.h"
#include "stadium/popen.h"
#include "stadium/punter.h"

namespace stadium {

class LocalPunter : public Punter {
 public:
  explicit LocalPunter(const std::string& shell);
  ~LocalPunter() override;

  PunterInfo Setup(int punter_id,
                   int num_punters,
                   const Map* map,
                   const Settings& settings) override;
  Move OnTurn(const std::vector<Move>& moves) override;

 private:
  std::unique_ptr<base::Value> RunProcess(
      Popen* subprocess,
      const base::DictionaryValue& request,
      std::string* out_name,
      const base::TimeDelta& timeout);

  const std::string shell_;

  int punter_id_;
  std::unique_ptr<base::Value> state_;
  std::unique_ptr<Popen> subprocess_;

  DISALLOW_COPY_AND_ASSIGN(LocalPunter);
};

}  // namespace stadium

#endif  // STADIUM_LOCAL_PUNTER_H_
