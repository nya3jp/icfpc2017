#include "punter/gamemapforai.h"
#include "punter/greedy_to_jam.h"

#include "base/memory/ptr_util.h"

#include <vector>
#include <algorithm>

using namespace std;

namespace punter {

GreedyToJam::GreedyToJam()
{
}

GreedyToJam::~GreedyToJam() = default;

void GreedyToJam::SetUp(const common::SetUpData& args) {
  themap.init(args.num_punters, args.game_map);
  SimplePunter::SetUp(args);
}

framework::GameMove GreedyToJam::Run() {
  // do nothing
  return {framework::GameMove::Type::PASS };
}
framework::GameMove GreedyToJam::Run(const std::vector<framework::GameMove>& moves) {
  LOG(INFO) << "Me: " << punter_id_;
  LOG(INFO) << "Scores: ";
  for(const auto& s: themap.getScores()) {
    LOG(INFO) << s;
  }
  for(auto& move: moves) {
    if(move.type == framework::GameMove::Type::CLAIM) {
      int source = move.source;
      int target = move.target;
      gameutil::GameMapForAI::node_index srcix = themap.id2ix.at(source);
      gameutil::GameMapForAI::node_index trgix = themap.id2ix.at(target);
      int color = move.punter_id;
      LOG(INFO) << "Moves:" << source << " " << target << " " << color;
      
      int plusscore = themap.claim(srcix, trgix, color);
      LOG(INFO) << "Plusscore" << plusscore;
    }else if(move.type == framework::GameMove::Type::SPLURGE) {
      int pid = move.route[0];
      int p_ix = themap.id2ix.at(pid);
      int color = move.punter_id;
      for(size_t ix = 1; ix < move.route.size(); ix++) {
        int q_ix = themap.id2ix.at(move.route[ix]);
        themap.claim(p_ix, q_ix, color);
        p_ix = q_ix;
      }
    }
  }
  LOG(INFO) << "Scores after update: ";
  for(const auto& s: themap.getScores()) {
    LOG(INFO) << s;
  }

  int rival = -1;
  vector<pair<int, int> > rivals;
  for(int p = 0; p < (int)themap.getScores().size(); ++p) {
    if(p != punter_id_) {
      rivals.emplace_back(make_pair(themap.getScores()[p], p));
    }
  }
  sort(rivals.begin(), rivals.end());
  if(!rivals.empty()) {
    if(rivals.back().first < themap.getScores()[punter_id_]
       || rivals.size() == 1) {
      rival = rivals.back().second;
    }else{
      rival = rivals[rivals.size() - 2].second;
    }
  }

  LOG(INFO) << "Rival is " << rival;

  movecandidate attackmove = {};
  int attackdamage = 0;
  if(rival >= 0) {
    auto candidates = list_candidates(rival);
    if(candidates.size() >= 2) {
      auto rivalbest = candidates[0];
      auto rivalsecondbest = candidates[1];
      LOG(INFO) << "Rival best gain = " << rivalbest.score;
      LOG(INFO) << "Rival 2nd best gain = " << rivalsecondbest.score;
      attackdamage = rivalbest.score - rivalsecondbest.score;
      attackmove = rivalbest;
      attackdamage += themap.delta_score(rivalbest.sourceix, rivalbest.targetix, punter_id_);
    }
  }
  LOG(INFO) << "Attack damage = " << attackdamage;
  LOG(INFO) << "Candidate edge = " << attackmove.sourceix << "->" << attackmove.targetix;

  auto mycandidates = list_candidates(punter_id_);
  if(mycandidates.empty()) {
    return {framework::GameMove::Type::PASS};
  }
  int sourceix;
  int targetix;
  LOG(INFO) << "Candidate score " << mycandidates[0].score;
  if(attackdamage > mycandidates[0].score) {
    sourceix = attackmove.sourceix;
    targetix = attackmove.targetix;
    LOG(INFO) << "Attack better!";
  }else{
    movecandidate mybest = mycandidates[0];
    sourceix = mybest.sourceix;
    targetix = mybest.targetix;
  }
  int source = themap.getNodeInfo()[sourceix].id;
  int target = themap.getNodeInfo()[targetix].id;

  
  return {framework::GameMove::Type::CLAIM, punter_id_, source, target };
}

vector<GreedyToJam::movecandidate> GreedyToJam::list_candidates(int color)
{
  vector<GreedyToJam::movecandidate> deltas;
  for(size_t ix = 0; ix < themap.getEdgeInfo().size(); ++ix) {
    const auto &e = themap.getEdgeInfo()[ix];
    if(e.color == -1) {
      int sc = themap.delta_score(e.source, e.dest, color);
      deltas.emplace_back(movecandidate({ sc, (int)ix, e.source, e.dest }));
    }
  }
  
  std::sort(deltas.begin(), deltas.end(),
            [](const GreedyToJam::movecandidate& a,
               const GreedyToJam::movecandidate& b)
            { return (a.score > b.score); });
    
  return std::move(deltas);
}

void GreedyToJam::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));

  // Deserialize punter-specific state here.

  string serialized;
  CHECK(state->GetString("themap", &serialized));
  istringstream is(serialized);
  themap.deserialize(is);

  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> GreedyToJam::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  // Serialize punter-specific state here.

  ostringstream os;
  themap.serialize(os);
  state->SetString("themap", os.str());

  return std::move(state);
}

} // namespace punter
