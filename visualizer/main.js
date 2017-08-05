const INF = Math.pow(10, 18);
let gGameMap;
let gPrecomputedData;
let gHistory;
let gScreen;
let gState;

// Static map data
function GameMap(sites, rivers, mines) {
  this.sites = [];
  this.rivers = [];
  this.mines = [];
  this.distance = [];  // distance[i][j]: distance from i-th mine to j-th site.
  this.siteId2Index_ = new Map();
  this.siteIndex2Id_ = new Map();
  this.initialize(sites, rivers, mines);
};

GameMap.prototype = {
  initialize: function(sites, rivers, mines) {
    // Keep map for siteId <=> siteIndex
    for (let i = 0; i < sites.length; i++) {
      if (!this.siteId2Index_.has(sites[i].id)) {
        const index = this.siteId2Index_.size;
        this.siteId2Index_.set(sites[i].id, index);
        this.siteIndex2Id_.set(index, sites[i].id);
      }
    }
    // Replace siteId with siteIndex in sites, rivers, and mines.
    for (let i = 0; i < sites.length; i++) {
      this.sites.push({id: this.siteId2Index_.get(sites[i].id),
                       x: sites[i].x, y: sites[i].y});
    }
    for (let i = 0; i < rivers.length; i++) {
      this.rivers.push({source: this.siteId2Index_.get(rivers[i].source),
                        target: this.siteId2Index_.get(rivers[i].target)});
    }
    for (let i = 0; i < mines.length; i++) {
      this.mines.push(this.siteId2Index_.get(mines[i]));
    }
  },

  getSiteIndex: function(siteId) {
    return this.siteId2Index_.get(siteId);
  },

  getSiteId: function(siteIndex) {
    return this.siteIndex2Id_.get(siteId);
  },
};

function PrecomputedData(gameMap) {
  this.adjacent = [];
  this.distance = [];
  this.precompute(gameMap);
};

PrecomputedData.prototype = {
  precompute: function(gameMap) {
    const sites = gameMap.sites;
    const rivers = gameMap.rivers;
    const mines = gameMap.mines || [];

    this.adjacent = new Array(sites.length);
    for (let i = 0; i < this.adjacent.length; i++)
      this.adjacent[i] = [];
    for (let i = 0; i < rivers.length; i++) {
      const src = rivers[i].source;
      const tgt = rivers[i].target;
      this.adjacent[src].push(tgt);
      this.adjacent[tgt].push(src);
    }

    this.distance = new Array(mines.length);
    for (let i = 0; i < mines.length; i++) {
      this.distance[i] = new Array(sites.length);
      for (let j = 0; j < sites.length; j++)
        this.distance[i][j] = INF;
      
      const visited = new Array(sites.length);
      const S = mines[i];
      const queue = [];
      visited[S] = true;
      this.distance[i][S] = 0;
      queue.push({v: S, d: 0});
      
      while (queue.length != 0) {
        const v = queue[0].v;
        const d = queue[0].d;
        queue.shift();
        for (let j = 0; j < this.adjacent[v].length; j++) {
          const v2 = this.adjacent[v][j];
          if (!visited[v2]) {
            visited[v2] = true;
            this.distance[i][v2] = d + 1;
            queue.push({v: v2, d: d+1});
          }
        }
      }
    }
  },
};

function History(moves, gameMap) {
  console.log('History move', moves);
  this.moves = moves;
  for (let i = 0; i < this.moves.length; i++) {
    if (this.moves[i].claim) {
      this.moves[i].claim.source =
          gameMap.getSiteIndex(this.moves[i].claim.source);
      this.moves[i].claim.target =
          gameMap.getSiteIndex(this.moves[i].claim.target);
    }
  }
  console.log('this.moves', this.moves);
};

History.prototype = {
  getPlayerSize: function() {
    
  },
};

function State() {
  this.step = 0;
}

State.prototype = {
  init: function() {
    this.step = 0;
  },
};

function Screen(canvas, scoreLabelContainer, prevButton, nextButton) {
  this.prevButton = prevButton;
  this.nextButton = nextButton;
  this.canvas_ = canvas;
  this.scoreLabelContainer_ = scoreLabelContainer;
}

Screen.prototype = {
  display: function(gameMap, history, state) {
    console.log(state);
    if (!gameMap)
      return;

    const sites = gameMap.sites || [];
    const rivers = gameMap.rivers || [];
    const mines = gameMap.mines || [];

    let minX, maxX, minY, maxY;
    minX = minY = INF;
    maxX = maxY = -INF;
    for (let i = 0; i < sites.length; i++) {
      minX = Math.min(minX, sites[i].x);
      maxX = Math.max(maxX, sites[i].x);
      minY = Math.min(minY, sites[i].y);
      maxY = Math.max(maxY, sites[i].y);
    }
    if (maxX == minX)
      maxX = minX + 1;
    if (maxY == minY)
      maxY = minY + 1;

    // Adjust canvas
    const scaleX = document.documentElement.clientWidth * 0.9 / (maxX - minX);
    const scaleY = document.documentElement.clientHeight * 0.8 / (maxY - minY);
    const scale = Math.min(scaleX, scaleY);
    this.canvas_.width = (maxX - minX) * scale;
    this.canvas_.height = (maxY - minY) * scale;

    const sitePX = new Array(sites.length);
    const sitePY = new Array(sites.length);
    for (let i = 0; i < sites.length; i++) {
      sitePX[i] = this.convertX_(sites[i].x, minX, maxX, this.canvas_.width);
      sitePY[i] = this.convertY_(sites[i].y, minY, maxY, this.canvas_.height);
    }
    const isMine = new Array(sites.length);
    for (let i = 0; i < mines.length; i++) {
      isMine[mines[i]] = true;
    }

    let players;
    let colors;
    if (history) {
      // Assign colors to players
      players = new Set();
      for (let i = 0; i < history.moves.length; i++) {
        let punterId;
        if (history.moves[i].claim)
          punterId = history.moves[i].claim.punter;
        else if (history.moves[i].pass)
          punterId = history.moves[i].pass.punter;
        if (punterId != undefined)
          players.add(punterId);
      }
      colors = new Array(players.size);
      for (let i = 0; i < colors.length; i++) {
        const hue = (0.4 + i / colors.length) / 1.0;
        const rgb = hslToRgb(hue, 1.0, 0.5);
        colors[i] = 'rgb(' + rgb[0] + ',' + rgb[1] + ',' + rgb[2] + ')';
      }
    }

    const ctx = this.canvas_.getContext("2d");

    // Draw rivers
    ctx.lineWidth = 1;
    for (let i = 0; i < rivers.length; i++) {
      ctx.beginPath();
      ctx.strokeStyle = 'rgb(120, 120, 120)';
      const src = rivers[i].source;
      const tgt = rivers[i].target;
      ctx.moveTo(sitePX[src], sitePY[src]);
      ctx.lineTo(sitePX[tgt], sitePY[tgt]);
      ctx.stroke();
    }

    // Draw claimed rivers
    for (let i = 0; i < state.step; i++) {
      const claim = history.moves[i].claim;
      if (!claim)
        continue;

      ctx.beginPath();

      ctx.strokeStyle = colors[claim.punter];
      ctx.moveTo(sitePX[claim.source], sitePY[claim.source]);
      ctx.lineTo(sitePX[claim.target], sitePY[claim.target]);
      ctx.lineWidth = i == state.step - 1 ? 5 : 2;
      ctx.stroke();
    }

    // Draw sites
    for (let i =0; i < sitePX.length; i++) {
      ctx.beginPath();
      ctx.arc(sitePX[i], sitePY[i], isMine[i] ? 6 : 3, 2 * Math.PI, false);
      ctx.fillStyle = isMine[i] ? 'red' : 'gray';
      ctx.fill();
    }

    console.log('history', history);
    console.log('state', state);

    if (history) {
      // Update buttons
      this.prevButton.disabled = !(state.step > 0);
      this.nextButton.disabled = !(state.step < history.moves.length);
      console.log('state', state);

      // Update score

      // edges[p][i]: punter p's i-th edge.
      const edges = new Array(players.size);
      for (let i = 0; i < edges.length; i++)
        edges[i] = [];
      for (let i = 0; i < state.step; i++) {
        if (!history.moves[i].claim)
          continue;
        const punter = history.moves[i].claim.punter;
        const src = history.moves[i].claim.source;
        const dst = history.moves[i].claim.target;
        edges[punter].push({src: src, dst: dst});
      }

      const scores = new Array(edges.length);
      for (let p = 0; p < edges.length; p++) {
        // Collect adjacent nodes.
        const punter = p; 

        scores[p] = 0;
        for (let i = 0; i < mines.length; i++)
          scores[p] += computeScore(i, edges, punter, sites.length, mines);
        console.log('Punter ' + p + ' = ' + scores[p]);
      }
      this.scoreLabelContainer_.textContent = '';
      for (let i = 0; i < scores.length; i++) {
        const el = document.createElement('span');
        el.textContent = scores[i];
        el.style.color = colors[i];
        this.scoreLabelContainer_.appendChild(el);
      }
    }
  },
  
  convertX_: function(x, minX, maxX, canvasWidth) {
    return (x - minX) * (canvasWidth * 0.9) / (maxX - minX) + canvasWidth * 0.05;
  },

  convertY_: function(y, minY, maxY, canvasHeight) {
    return (y - minY) * (canvasHeight * 0.9) / (maxY - minY) + canvasHeight * 0.05;
  },
};

function hslToRgb(h, s, l){
  let r, g, b;
  if (s == 0) {
    r = g = b = l; // achromatic
  } else {
    const hue2rgb = function hue2rgb(p, q, t){
      if(t < 0) t += 1;
      if(t > 1) t -= 1;
      if(t < 1/6) return p + (q - p) * 6 * t;
      if(t < 1/2) return q;
      if(t < 2/3) return p + (q - p) * (2/3 - t) * 6;
      return p;
    }
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    r = hue2rgb(p, q, h + 1/3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1/3);
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

function readMap(map) {
  gGameMap = new GameMap(map.sites, map.rivers, map.mines);
  gPrecomputedData = new PrecomputedData(gGameMap);
  gScreen.display(gGameMap, gHistory, gState);
}

function readMoves(moves) {
  gHistory = new History(moves.moves, gGameMap);
  gScreen.display(gGameMap, gHistory, gState);
}

function computeScoreRec(v, mineIndex, adj, depth, visited, mines) {
  if (visited[v])
    return 0;
  
  visited[v] = true;
  score = gPrecomputedData.distance[mineIndex][v] * gPrecomputedData.distance[mineIndex][v];
  for (let i = 0; i < adj[v].length; i++) {
    const v2 = adj[v][i];
    score += computeScoreRec(v2, mineIndex, adj, depth, visited, mines);
  }
  return score;
}

function computeScore(mineIndex, edges, punter, V, mines) {
  const visited = new Array(V);
  const adj = new Array(V);
  for (let i = 0; i < adj.length; i++)
    adj[i] = [];

  for (let i = 0; i < edges[punter].length; i++) {
    const src = edges[punter][i].src;
    const dst = edges[punter][i].dst;
    adj[src].push(dst);
    adj[dst].push(src);
  }
  return computeScoreRec(mines[mineIndex], mineIndex, adj, 0, visited, mines);
}

function onPrev() {
  gState.step = Math.max(gState.step - 1, 0);
  gScreen.display(gGameMap, gHistory, gState);
}

function onNext() {
  gState.step = Math.min(gState.step + 1, gHistory.moves.length);
  gScreen.display(gGameMap, gHistory, gState);
}

document.addEventListener('DOMContentLoaded', function(event) {
  gScreen = new Screen(document.getElementById('canvas'),
                       document.getElementById('scores'),
                       document.getElementById('prev'),
                       document.getElementById('next'));
  gState = new State();

  const inputMap = document.getElementById('map');
  inputMap.addEventListener('change', function(event) {
    const reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      const map = JSON.parse(event.target.result);
      readMap(map);
    };
  });

  const inputMoves = document.getElementById('moves');
  inputMoves.addEventListener('change', function(event) {
    gState.init();
    const reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      const moves = JSON.parse(event.target.result);
      readMoves(moves);
    };
  });

  gScreen.prevButton.addEventListener('click', onPrev);
  gScreen.nextButton.addEventListener('click', onNext);
});
