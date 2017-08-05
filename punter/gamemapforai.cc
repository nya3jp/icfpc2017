#include "punter/gamemapforai.h"

#include <queue>
#include <stack>

namespace gameutil {

using namespace std;
using base::Optional;

GameMapForAI::GameMapForAI(int num_punters,
                           const framework::GameMap& game_map)
{
  init(num_punters, game_map);
}

void GameMapForAI::init(int num_punters, const framework::GameMap& game_map)
{
  num_punters_ = num_punters;
  nodeinfo_.clear();
  edgeinfo_.clear();
  themap_.clear();
  nodeinfo_.resize(game_map.sites.size());
  edgeinfo_.resize(game_map.rivers.size());
  themap_.resize(game_map.sites.size());

  for(size_t i = 0; i < game_map.sites.size(); ++i) {
    int id = game_map.sites[i].id;
    nodeinfo_[i].id = id;
    id2ix[id] = i;
  }

  size_t ix = 0;
  for(const auto& e: game_map.rivers) {
    node_index source = id2ix.at(e.source);
    node_index dest = id2ix.at(e.target);
    
    if(source > dest){ std::swap(source, dest); }

    themap_[source].push_back(make_pair(dest, ix));
    themap_[dest].push_back(make_pair(source, ix));

    edgeinfo_[ix].source = source;
    edgeinfo_[ix].dest = dest;
    edgeinfo_[ix].color = -1;

    ix++;
  }

  mines_.clear();
  for(const auto& m: game_map.mines) {
    mines_.push_back(id2ix.at(m));
  }

  for(auto &n: nodeinfo_) {
    n.minedist = vector<int>(mines_.size(), 0);
    n.reachable = vector<vector<bool> >(mines_.size(), vector<bool>(num_punters, false));
  }

  for(size_t mineix = 0; mineix < game_map.mines.size(); ++mineix) {
    int m = mines_[mineix];
    nodeinfo_[m].reachable[mineix] = vector<bool>(num_punters, true);
  }

  initMineScores();
}

void GameMapForAI::initMineScores()
{
  const auto& mines = getMines();
  for(size_t mineix = 0; mineix < mines.size(); ++mineix) {
    int m = mines[mineix];

    queue<node_index> q;
    q.push(m);

    while(!q.empty()) {
      int v = q.front();
      q.pop();
      int curscore = nodeinfo_[v].minedist[mineix];

      for(const auto &e: themap_[v]) {
        int adj = e.first;
        if(adj != m && nodeinfo_[adj].minedist[mineix] == 0) {
          nodeinfo_[adj].minedist[mineix] = curscore + 1;
          q.push(adj);
        }
      }
    }
  }
}


int GameMapForAI::claim(node_index source, node_index dest, int punter)
{
  return claim_impl(source, dest, punter, true);
}

int GameMapForAI::delta_score(node_index source, node_index dest, int punter)
{
  return claim_impl(source, dest, punter, false);
}

int GameMapForAI::claim_impl(node_index source, node_index dest, int punter, bool claim)
{
  int ret = 0;
  
  DLOG(INFO) << "In claim impl" << (claim ? "" : "(query)") << ": source " << source;
  DLOG(INFO) << "In claim impl" << (claim ? "" : "(query)") << ": dest " << dest;

  for(size_t mineix = 0; mineix < mines_.size(); ++mineix) {
    DCHECK(nodeinfo_[source].reachable.size() > mineix);
    DCHECK(nodeinfo_[dest].reachable.size() > mineix);
    DCHECK(nodeinfo_[source].reachable[mineix].size() > punter);
    DCHECK(nodeinfo_[dest].reachable[mineix].size() > punter);
    bool source_reachable = nodeinfo_[source].reachable[mineix][punter];
    bool dest_reachable = nodeinfo_[dest].reachable[mineix][punter];
    if(source_reachable != dest_reachable) {
      // Either source or dest is not reachable and will turn reachable if connected.

      node_index node_to_be_enabled = source_reachable ? dest : source;
      node_index node_to_be_ignored = source_reachable ? source : dest;
      vector<bool> visited(nodeinfo_.size(), false);
      vector<node_index> newly_reachable;

      stack<node_index> s;
      s.push(node_to_be_enabled);
      while(!s.empty()) {
        int v = s.top();
        s.pop();
        newly_reachable.push_back(v);
        
        for(const auto &e: themap_[v]) {
          edge_index ei = e.second;
          const auto& eref = edgeinfo_[ei];
          int adj = e.first;
          if(!visited[adj] && adj != node_to_be_ignored &&
             eref.color == punter) {
            s.push(adj);
            visited[adj] = true;
          }
        }
      }

      for(node_index v: newly_reachable) {
        int d = nodeinfo_[v].minedist[mineix];
        ret += d * d;
      }

      if(claim){
        // Update edge scores.
        for(node_index v: newly_reachable) {
          nodeinfo_[v].reachable[mineix][punter] = true;
        }
        
        scores_[punter] += ret;
      }
    }
  }

  if(claim) {
    // Update edge color.
    bool updated = false;
    for(const auto &e: themap_[source]) {
      if(e.first != dest) continue;
      edgeinfo_[e.second].color = punter;
      updated = true;
      break;
    }
    if(!updated) {
      DLOG(WARNING) << "Claiming river does not exist: " << source << "->" << dest << " punter = " << punter;
    }
  }

  return ret;
}

std::ostream& GameMapForAI::serialize(std::ostream &os)
{
  os << num_punters_ << endl;
  os << mines_.size() << endl;

  os << nodeinfo_.size() << endl;
  for(const auto& n: nodeinfo_) {
    os << n.id << endl;
    for(size_t i = 0; i < mines_.size(); ++i) {
      os << n.minedist[i] << " ";
    }
    os << endl;
    for(size_t i = 0; i < mines_.size(); ++i) {
      for(size_t u = 0; u < (size_t)num_punters_; ++u) {
        os << (int) n.reachable[i][u] << " ";
      }
    }
    os << endl;
  }

  os << edgeinfo_.size() << endl;
  for(const auto& e: edgeinfo_) {
    os << e.source << " " << e.dest << " " << e.color << endl;
  }

  for(int m: mines_) {
    os << m << endl;
  }

  for(int s: scores_) {
    os << s << endl;
  }

  return os;
}

std::istream& GameMapForAI::deserialize(std::istream& is)
{
  is >> num_punters_;
  int num_mines;
  is >> num_mines;
  mines_.resize(num_mines);

  int nodesize;
  is >> nodesize;
  nodeinfo_.resize(nodesize);
  for(int i = 0; i < nodesize; ++i) {
    auto& node = nodeinfo_[i];
    is >> node.id;
    id2ix[node.id] = i;
    node.minedist.resize(num_mines);
    node.reachable.resize(num_mines, vector<bool>(num_punters_, false));
    for(int j = 0; j < num_mines; ++j) {
      is >> node.minedist[j];
      for(int k = 0; k < num_punters_; ++k) {
        int st;
        is >> st;
        node.reachable[j][k] = (st != 0);
      }
    }
  }

  themap_.resize(nodesize);

  int edgesize;
  is >> edgesize;
  edgeinfo_.resize(edgesize);
  for(int i = 0; i < edgesize; ++i) {
    auto& edge = edgeinfo_[i];
    is >> edge.source >> edge.dest >> edge.color;
    node_index source = edge.source;
    node_index dest = edge.dest;
    themap_[source].push_back(make_pair(dest, i));
    themap_[dest].push_back(make_pair(source, i));
  }

  for(int i = 0; i < num_mines; ++i) {
    is >> mines_[i];
  }

  scores_.resize(num_punters_);
  for(int i = 0; i < num_punters_; ++i) {
    is >> scores_[i];
  }

  return is;
}


} // namespace gameutil




