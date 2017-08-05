#ifndef PUNTER_GAMEUTIL_GAMEMAPFORAI_H_
#define PUNTER_GAMEUTIL_GAMEMAPFORAI_H_

#include <vector>
#include <iostream>
#include <map>

#include "framework/game.h"
#include "base/optional.h"

namespace gameutil {

class GameMapForAI
{
 public:
  GameMapForAI() = default;
  GameMapForAI(int num_punters,
	       const framework::GameMap& game_map);

  typedef int node_index;
  typedef int edge_index;

  struct NodeInfo {
    int id;
    std::vector<int> minedist;
    std::vector<std::vector<bool> > reachable; // size [mine][user]
  };

  struct EdgeInfo {
    node_index source, dest;
    int color;
  };

  /**
     @brief Claim a river.
     @args source Source node.
     @args dest Destination node.
     @args punter Punter id.
     @return score difference
   */
  int claim(node_index source, node_index dest, int punter);

  /**
     @brief Return the increase in the score when an edge was claimed by a user. Undefined behaviour if this edge is already claimed by that punter.
     @args source Source node.
     @args dest Destination node.
     @args punter Punter id.
     @return increase in score.
  */
  int delta_score(node_index source, node_index dest, int punter);

  const std::vector<int>& getScores() const { return scores_; }

  const std::vector<node_index>& getMines() const { return mines_; }

  const std::vector<NodeInfo>& getNodeInfo() const { return nodeinfo_; }
  const std::vector<EdgeInfo>& getEdgeInfo() const { return edgeinfo_; }
  
  std::ostream& serialize(std::ostream& os);
  std::istream& deserialize(std::istream& is);

  void init(int num_punters, const framework::GameMap& game_map);

  std::map<int, node_index> id2ix;
  
 private:
  int num_punters_;

  int claim_impl(node_index source, node_index dest, int punter, bool claim);

  typedef std::vector<std::vector<std::pair<node_index, size_t> > > Graph;

  std::vector<NodeInfo> nodeinfo_;
  std::vector<EdgeInfo> edgeinfo_;

  Graph themap_;
  
  std::vector<node_index> mines_;

  std::vector<base::Optional<node_index> > future_;
  
  std::vector<int> scores_;

  void initMineScores();
    
};

} // namespace gameutil

#endif // PUNTER_GAMEUTIL_GAMEMAPFORAI_H_
