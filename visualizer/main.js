const INF = Math.pow(10, 18);

// Static map data
function GameMap() {
  this.reset();
};

GameMap.prototype = {
  reset: function() {
    this.sites = [];
    this.rivers = [];
    this.mines = [];
    this.distance = [];  // distance[i][j]: distance from i-th mine to j-th site.
    this.adjacent = [];  // adjacent[i][j]: i-th node's j-th edge's destination.
    this.siteId2Index_ = new Map();
    this.siteIndex2Id_ = new Map();
  },

  load: function(map) {
    this.reset();
    this.initialize(map.sites, map.rivers, map.mines || []);
    this.precompute_();
  },

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

  precompute_: function() {
    const sites = this.sites;
    const rivers = this.rivers;
    const mines = this.mines || [];

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
          this.distance[i][v2] = Math.min(this.distance[i][v2], d + 1);
          if (!visited[v2]) {
            visited[v2] = true;
            queue.push({v: v2, d: d+1});
          }
        }
      }
    }
  },
};

function History() {
  this.reset();
};

History.prototype = {
  reset: function() {
    this.moves = [];
  },

  load: function(moves, gameMap) {
    this.reset();
    this.moves = moves;
    for (let i = 0; i < this.moves.length; i++) {
      if (this.moves[i].claim) {
        this.moves[i].claim.source =
            gameMap.getSiteIndex(this.moves[i].claim.source);
        this.moves[i].claim.target =
            gameMap.getSiteIndex(this.moves[i].claim.target);
      }
      if (this.moves[i].option) {
        this.moves[i].option.source =
            gameMap.getSiteIndex(this.moves[i].option.source);
        this.moves[i].option.target =
            gameMap.getSiteIndex(this.moves[i].option.target);
      }
    }
  },
};

function State() {
  this.reset();
}

State.prototype = {
  reset: function() {
    this.step = 0;
  },
};

function Scorer(gameMap) {
  this.gameMap_ = gameMap;
}

Scorer.prototype = {
  compute: function(edges) {
    const mines = this.gameMap_.mines;
    let score = 0;
    for (let i = 0; i < mines.length; i++) {
      score += this.computeForMine_(edges, i);
    }
    return score;
  },

  computeForMine_: function(edges, mineIndex) {
    const sites = this.gameMap_.sites;
    const V = sites.length;
    const visited = new Array(V);
    const adj = new Array(V);
    for (let i = 0; i < adj.length; i++)
      adj[i] = [];
    for (let i = 0; i < edges.length; i++) {
      const src = edges[i].src;
      const dst = edges[i].dst;
      adj[src].push(dst);
      adj[dst].push(src);
    }
    return this.computeRecursively_(this.gameMap_.mines[mineIndex], mineIndex, adj, visited);
  },

  computeRecursively_: function(v, mineIndex, adj, visited) {
    if (visited[v])
      return 0;

    visited[v] = true;
    let score = this.gameMap_.distance[mineIndex][v] * this.gameMap_.distance[mineIndex][v];
    for (let i = 0; i < adj[v].length; i++) {
      const v2 = adj[v][i];
      score += this.computeRecursively_(v2, mineIndex, adj, visited);
    }
    return score;
  },
};

function computeScoreRec(v, mineIndex, adj, depth, visited, mines, precomputedData) {
  if (visited[v])
    return 0;
  
  visited[v] = true;
  score = precomputedData.distance[mineIndex][v] * precomputedData.distance[mineIndex][v];
  for (let i = 0; i < adj[v].length; i++) {
    const v2 = adj[v][i];
    score += computeScoreRec(v2, mineIndex, adj, depth, visited, mines, precomputedData);
  }
  return score;
}

function computeScore(mineIndex, edges, punter, V, mines, precomputedData) {
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
  return computeScoreRec(mines[mineIndex], mineIndex, adj, 0, visited, mines, precomputedData);
}


function Viewer(canvas, scoreLabelContainer, prevButton, nextButton, autoPlayButton, slider, movesInput) {
  this.prevButton = prevButton;
  this.nextButton = nextButton;
  this.autoPlayButton = autoPlayButton;
  this.canvas_ = canvas;
  this.scoreLabelContainer_ = scoreLabelContainer;
  this.slider_ = slider;
  this.movesInput_ = movesInput;
}

Viewer.prototype = {
  display: function(gameMap, history, state, scorer) {
    // Disable/enable elements based on available data.
    this.movesInput_.disabled = !gameMap;
    this.prevButton.disabled = !history || !(state.step > 0);
    this.nextButton.disabled = !history || !(state.step < history.moves.length);
    this.autoPlayButton.disabled = !history || !(history.moves.length > 0);
    this.slider_.disabled = !history || !(history.moves.length > 0);
    this.slider_.max = history.moves.length;
    this.slider_.MaterialSlider.change(state.step);

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
        if (history.moves[i].option)
          punterId = history.moves[i].option.punter;
        else if (history.moves[i].pass)
          punterId = history.moves[i].pass.punter;

        if (punterId != undefined)
          players.add(punterId);
      }
      colors = new Array(players.size);
      for (let i = 0; i < colors.length; i++) {
        const hue = (0.4 + i / colors.length) / 1.0;
        const rgb = this.hslToRgb_(hue, 1.0, 0.5);
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

    const V = sites.length;
    const taken = {};  // taken[src*V + dst] => true if river(src,tgt) is taken.

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
      taken[claim.source * V + claim.target] = true;
      taken[claim.target * V + claim.source] = true;
    }

    // Draw option rivers.
    for (let i = 0; i < state.step; i++) {
      const option = history.moves[i].option;
      if (!option)
        continue;

      let x1 = sitePX[option.source];
      let y1 = sitePY[option.source];
      let x2 = sitePX[option.target];
      let y2 = sitePY[option.target];
      if (x1 == x2 && y1 == y2)
        continue;
      if (taken[option.source * V + option.target]) {
        const dx = x2 - x1;
        const dy = y2 - y1;
        const len = Math.sqrt(dx*dx + dy*dy);
        const dx2 = (dy / len) * 3;
        const dy2 = (-dx / len) * 3;
        x1 += dx2;
        y1 += dy2;
        x2 += dx2;
        y2 += dy2;
      } else {
        taken[option.source * V + option.target] = true;
        taken[option.target * V + option.source] = true;
      }
      ctx.beginPath();
      ctx.strokeStyle = colors[option.punter];
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineWidth = i == state.step - 1 ? 5 : 2;
      ctx.stroke();
    }

    // Draw splurged rivers
    for (let i = 0; i < state.step; i++) {
      const splurge = history.moves[i].splurge;
      if (!splurge)
        continue;

      for (let j = 0; j < splurge.route.length - 1; j++) {
        let src = splurge.route[j];
        let tgt = splurge.route[j+1];
        let x1 = sitePX[src];
        let y1 = sitePY[src];
        let x2 = sitePX[tgt];
        let y2 = sitePY[tgt];
        if (x1 == x2 && y1 == y2)
          continue;
        if (taken[src * V + tgt]) {
          const dx = x2 - x1;
          const dy = y2 - y1;
          const len = Math.sqrt(dx*dx + dy*dy);
          const dx2 = (dy / len) * 3;
          const dy2 = (-dx / len) * 3;
          x1 += dx2;
          y1 += dy2;
          x2 += dx2;
          y2 += dy2;
        } else {
          taken[src * V + tgt] = true;
          taken[tgt * V + src] = true;
        }
        ctx.beginPath();
        ctx.strokeStyle = colors[splurge.punter];
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.lineWidth = i == state.step - 1 ? 5 : 2;
        ctx.stroke();
      }
    }

    // Draw sites
    for (let i =0; i < sitePX.length; i++) {
      ctx.beginPath();
      ctx.arc(sitePX[i], sitePY[i], isMine[i] ? 6 : 3, 2 * Math.PI, false);
      ctx.fillStyle = isMine[i] ? 'red' : 'gray';
      ctx.fill();
    }

    if (history) {
      // Update score
      // edges[p][i]: punter p's i-th edge.
      const edges = new Array(players.size);
      for (let i = 0; i < edges.length; i++)
        edges[i] = [];
      for (let i = 0; i < state.step; i++) {
        if (history.moves[i].claim) {
          const punter = history.moves[i].claim.punter;
          const src = history.moves[i].claim.source;
          const dst = history.moves[i].claim.target;
          edges[punter].push({src: src, dst: dst});
        }
        if (history.moves[i].option) {
          const punter = history.moves[i].option.punter;
          const src = history.moves[i].option.source;
          const dst = history.moves[i].option.target;
          edges[punter].push({src: src, dst: dst});
        }
        if (history.moves[i].splurge) {
          const punter = history.moves[i].splurge.punter;
          for (let j = 0; j < history.moves[i].splurge.route.length-1; j++) {
            const src = history.moves[i].splurge.route[j];
            const dst = history.moves[i].splurge.route[j+1];
            edges[punter].push({src: src, dst: dst});
          }
        }
      }

      const scores = new Array(edges.length);
      for (let i = 0; i < edges.length; i++)
        scores[i] = scorer.compute(edges[i]);

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

  hslToRgb_: function(h, s, l){
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
  },
};

function Controller(prevButton, playButton, nextButton, slider, inputMap, inputMoves, viewer) {
  this.playing = false;
  this.prevButton_ = prevButton;
  this.playButton_ = playButton;
  this.nextButton_ = nextButton;
  this.slider_ = slider;
  this.inputMap_ = inputMap;
  this.inputMoves_ = inputMoves;
  this.viewer_ = viewer;
  this.gameMap_ = new GameMap();
  this.history_ = new History();
  this.state_ = new State();
  this.scorer_ = new Scorer(this.gameMap_);
  this.timerId_ = -1;

  this.prevButton_.addEventListener('click', this.onPrevClicked_.bind(this));
  this.playButton_.addEventListener('click', this.onPlayClicked_.bind(this));
  this.nextButton_.addEventListener('click', this.onNextClicked_.bind(this));
  this.slider_.addEventListener('input', this.onSliderChange_.bind(this));
  this.inputMap_.addEventListener('change', this.onInputMapChange_.bind(this));
  this.inputMoves_.addEventListener('change', this.onInputMovesChange_.bind(this));
}

Controller.prototype = {
  reset: function() {
    this.playing = false;
    this.state_.step = 0;
  },

  updateView_: function() {
    this.viewer_.display(this.gameMap_, this.history_, this.state_, this.scorer_);
  },

  startPlay_: function() {
    if (this.timerId_ != -1)
      clearInterval(this.timerId_);
    if (this.state_.step == this.history_.moves.length)
      this.state_.step = 0;
    this.timerId_ = setInterval(this.onInterval_.bind(this), 50);
    this.playButton_.querySelector('i').textContent = 'pause';
    this.playing = true;
  },

  stopPlay_: function() {
    if (this.timerId_ != -1) {
      clearInterval(this.timerId_);
      this.timerId_ = -1;
    }
    this.playButton_.querySelector('i').textContent = 'play_arrow';
    this.playing = false;
  },

  onPlayClicked_: function(event) {
    if (this.playing)
      this.stopPlay_();
    else
      this.startPlay_();
  },

  onInterval_: function() {
    this.onNextClicked_();
    if (this.state_.step == this.history_.moves.length)
      this.stopPlay_();
  },

  onPrevClicked_: function() {
    this.state_.step = Math.max(this.state_.step - 1, 0);
    this.updateView_();
  },

  onNextClicked_: function() {
    this.state_.step = Math.min(this.state_.step + 1, this.history_.moves.length);
    this.updateView_();
  },

  onSliderChange_: function(event) {
    if (this.playing)
      this.stopPlay_();

    const step = parseInt(event.target.value, 10);
    if (step >= 0 && step <= this.history_.moves.length) {
      this.state_.step = step;
      this.updateView_();
    }
  },

  onInputMapChange_: function(event) {
    const reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      const map = JSON.parse(event.target.result);
      this.gameMap_.load(map);
      this.history_.reset();
      this.state_.reset();
      this.updateView_();
    }.bind(this);
  },

  onInputMovesChange_: function(event) {
    const reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      const moves = JSON.parse(event.target.result);
      this.history_.load(moves.moves, this.gameMap_);
      this.state_.reset();
      this.updateView_();
    }.bind(this);
  },
};

document.addEventListener('DOMContentLoaded', function(event) {
  const $ = function( id ) { return document.getElementById( id ); };
  const viewer = new Viewer($('canvas'), $('scores'), $('prev'), $('next'),
      $('btn_play'), $('slider'), $('moves'));
  const controller = new Controller($('prev'), $('btn_play'), $('next'), 
      $('slider'), $('map'), $('moves'), viewer);
});
