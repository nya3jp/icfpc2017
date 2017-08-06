#include "punter/benkei.h"

#include <queue>
#include <vector>

#include "base/memory/ptr_util.h"
#include "framework/simple_punter.h"

namespace punter {

using namespace std;

Benkei::Benkei() {}
Benkei::~Benkei() = default;

framework::GameMove Benkei::Run() {
  while(getBenkeiProto()->chokepoints_size() != 0) {
    int ix = getBenkeiProto()->chokepoints(getBenkeiProto()->chokepoints_size() - 1);
    getBenkeiProto()->mutable_chokepoints()->RemoveLast();
    if(IsRiverClaimed(ix)) {
      continue;
    }
    return framework::GameMove({framework::GameMove::Type::CLAIM, punter_id_, rivers_->Get(ix).source(), rivers_->Get(ix).target()});
  }
  
  return GreedyPunterMirac::Run();
}

bool Benkei::IsRiverClaimed(int river_ix) {
  return rivers_->Get(river_ix).punter() != -1;
}

bool Benkei::IsRiverClaimed(int source, int dest) {
  for(const auto &e: edges_[source]) {
    int ix = e.river;
    if((rivers_->Get(ix).source() == source && rivers_->Get(ix).target() == dest) ||
       (rivers_->Get(ix).source() == dest && rivers_->Get(ix).target() == source)) {
      return IsRiverClaimed(ix);
    }
  }
  CHECK(false);
  //NOTREACHED
  return false;
}

void Benkei::SetUp(const common::SetUpData& args)
{
  SimplePunter::SetUp(args);

  // Estimated to complete within sub-seconds.
  // V = 5000 , E = 20000, M=100
  // O(E M) (bfs) + O(M * M * radius) (traceback). radius ~= sqrt(V))

  vector<int> freqs(rivers_->size(), 0);
  vector<int> mines_copy;
  for(int i = 0; i < mines_->size(); ++i) {
    mines_copy.push_back(mines_->Get(i).site());
  }
  for(int i = 0; i < mines_->size(); ++i) {
    frequentpaths(mines_->Get(i).site(), mines_copy, &freqs);
  }

  int threshold = mines_->size() * (mines_->size() - 1) / 4 + 1;

  for(size_t i = 0; i < freqs.size(); ++i) {
    if(freqs[i] >= threshold) {
      LOG(INFO) << "Bridge ix = " << i << ": " << rivers_->Get(i).source() << "->" << rivers_->Get(i).target();
      getBenkeiProto()->add_chokepoints(i);
    }
  }

  return;
}

void Benkei::frequentpaths(int origin, const vector<int>& target_sites, vector<int>* freqs)
{
  CHECK(freqs->size() == (size_t)rivers_->size());
  
  vector<bool> visited(num_sites(), false);
  vector<pair<int, int> > bp(num_sites(), make_pair(-1, -1));
  queue<int> q;
  q.push(origin);
  visited[origin] = true;

  // BFS
  while(!q.empty()) {
    int v = q.front();
    q.pop();

    for(const auto& e: edges_[v]) {
      int adj = e.site;
      int riverix = e.river;
      // const auto & r = rivers_[riverix];
      if(!visited[adj]) {
        q.push(adj);
        visited[adj] = true;
        bp[adj] = make_pair(v, riverix);
      }
    }
  }

  // trace bp and add freqs
  for(int s: target_sites) {
    int c = s;
    while(bp[c].first != -1) {
      int r = bp[c].second;
      ++((*freqs)[r]);
      c = bp[c].first;
    }
  }
  
  return;
}


} // namespace punter
