#ifndef PUNTER_JAMMER_H_
#define PUNTER_JAMMER_H_

#include "framework/simple_punter.h"

namespace punter {

class Jammer : public framework::SimplePunter {
 public:
  Jammer();
  ~Jammer() override;

  framework::GameMove Run() override;

 private:
  struct Candidate {
    int score;
    int source;
    int target;
  };
  std::vector<Candidate> list_candidates(int punter_id);
};

} // namespace punter

#endif  // PUNTER_JAM2_H_
