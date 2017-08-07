#include "punter/jammer.h"

#include <vector>
#include "common/scorer.h"

namespace punter {

Jammer::Jammer() = default;
Jammer::~Jammer() = default;

framework::GameMove Jammer::Run() {
  int rival = -1;
  std::vector<std::pair<int, int>> rivals;  // score, pid
  for (int p = 0; p < num_punters_; ++p) {
    if (p != punter_id_) {
      rivals.emplace_back(std::make_pair(GetScore(p), p));
    }
  }
  sort(rivals.begin(), rivals.end());
  if (!rivals.empty()) {
    if (rivals.back().first < GetScore(punter_id_)
       || rivals.size() == 1) {
      rival = rivals.back().second;
    } else {
      rival = rivals[rivals.size() - 2].second;
    }
  }

  DLOG(INFO) << "Rival is " << rival;

  common::Scorer scorer(proto_.mutable_scorer());

  Candidate attackmove = {};
  int attackdamage = 0;
  if (rival >= 0) {
    auto candidates = list_candidates(rival);
    if(candidates.size() >= 2) {
      auto rivalbest = candidates[0];
      auto rivalsecondbest = candidates[1];
      DLOG(INFO) << "Rival best gain = " << rivalbest.score;
      DLOG(INFO) << "Rival 2nd best gain = " << rivalsecondbest.score;
      attackdamage = rivalbest.score - rivalsecondbest.score;
      attackmove = rivalbest;
      attackdamage += scorer.TryClaim(punter_id_, rivalbest.source, rivalbest.target);
    }
  }
  DLOG(INFO) << "Attack damage = " << attackdamage;
  DLOG(INFO) << "Candidate edge = " << attackmove.source << "->" << attackmove.target;

  auto mycandidates = list_candidates(punter_id_);
  if (mycandidates.empty()) {
    return CreatePass();
  }

  int source;
  int target;
  DLOG(INFO) << "Candidate score " << mycandidates[0].score;
  if (attackdamage > mycandidates[0].score) {
    source = attackmove.source;
    target = attackmove.target;
    DLOG(INFO) << "Attack better!";
  } else {
    Candidate mybest = mycandidates[0];
    source = mybest.source;
    target = mybest.target;
  }

  return CreateClaim(source, target);
}

std::vector<Jammer::Candidate> Jammer::list_candidates(int punter_id)
{
  common::Scorer scorer(proto_.mutable_scorer());

  std::vector<Candidate> deltas;
  for (auto& river : *rivers_) {
    if (river.punter() == -1) {
      int sc = scorer.TryClaim(punter_id, river.source(), river.target());
      deltas.emplace_back(Candidate({ sc, river.source(), river.target() }));
    }
  }
  std::sort(deltas.begin(), deltas.end(),
            [](const Candidate& a, const Candidate& b) { return (a.score > b.score); });

  return std::move(deltas);
}

} // namespace punter
