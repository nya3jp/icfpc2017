#include "punter/gamemapforai.h"
#include "punter/greedy_punter_chun.h"

#include "base/memory/ptr_util.h"

#include <vector>

using namespace std;

namespace punter {

GreedyPunterChun::GreedyPunterChun()
{
}

GreedyPunterChun::~GreedyPunterChun() = default;

void GreedyPunterChun::SetUp(int punter_id, int num_punters, const framework::GameMap& game_map) {
  themap.init(num_punters, game_map);
  SimplePunter::SetUp(punter_id, num_punters, game_map);
}

framework::GameMove GreedyPunterChun::Run() {
  // do nothing
  return {framework::GameMove::Type::PASS };
}
framework::GameMove GreedyPunterChun::Run(const std::vector<framework::GameMove>& moves) {
  DLOG(INFO) << "Size of moves: " << moves.size();
  for(auto& move: moves) {
    if(move.type == framework::GameMove::Type::CLAIM) {
      int source = move.source;
      int target = move.target;
      DLOG(INFO) << "Src ix before conversion " << source;
      DLOG(INFO) << "Trg ix before conversion " << target;
      gameutil::GameMapForAI::node_index srcix = themap.id2ix.at(source);
      gameutil::GameMapForAI::node_index trgix = themap.id2ix.at(target);
      DLOG(INFO) << "Src ix after conversion " << srcix;
      DLOG(INFO) << "Trg ix after conversion " << trgix;
      int color = move.punter_id;

      themap.claim(srcix, trgix, color);
    }
  }
  
  typedef  gameutil::GameMapForAI::EdgeInfo EdgeInfo;
  vector<pair<int, const EdgeInfo*> > v;
  for(const auto &e: themap.getEdgeInfo()) {
    if(e.color == -1) {
      int sc = themap.delta_score(e.source, e.dest, punter_id_);
      v.emplace_back(make_pair(sc, &e));
    }
  }
  CHECK(v.size() >= 1);
  sort(v.begin(), v.end(),
       [](const pair<int, const EdgeInfo*> &a,
          const pair<int, const EdgeInfo*> &b) -> bool
       { return a.first < b.first; });

  const EdgeInfo *e = v.back().second;
  LOG(INFO) << "best score = " << v.back().first;
  int source = themap.getNodeInfo()[e->source].id;
  int target = themap.getNodeInfo()[e->dest].id;
  
  return {framework::GameMove::Type::CLAIM, punter_id_, source, target };
}

void GreedyPunterChun::SetState(std::unique_ptr<base::Value> state_in) {
  auto state = base::DictionaryValue::From(std::move(state_in));

  // Deserialize punter-specific state here.

  string serialized;
  CHECK(state->GetString("themap", &serialized));
  istringstream is(serialized);
  themap.deserialize(is);

  SimplePunter::SetState(std::move(state));
}

std::unique_ptr<base::Value> GreedyPunterChun::GetState() {
  auto state = base::DictionaryValue::From(SimplePunter::GetState());
  // Serialize punter-specific state here.

  ostringstream os;
  themap.serialize(os);
  state->SetString("themap", os.str());

  return std::move(state);
}

} // namespace punter
