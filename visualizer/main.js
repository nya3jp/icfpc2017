const INF = Math.pow(10, 18);
var wrapper;
var canvas;
var scoreContainer;
var btnPrev;
var btnNext;
var gMap;
var gMoves;
var gStep = 0;
var gDistance; // gDistance[m][v]: vertex v's  from mine m.
var gScoreLabels;

function convertX(x, minX, maxX, canvasWidth) {
  return (x - minX) * (canvasWidth * 0.9) / (maxX - minX) + canvasWidth * 0.05;
}

function convertY(y, minY, maxY, canvasHeight) {
  return (y - minY) * (canvasHeight * 0.9) / (maxY - minY) + canvasHeight * 0.05;
}

function hslToRgb(h, s, l){
  var r, g, b;
  if (s == 0) {
    r = g = b = l; // achromatic
  } else {
    var hue2rgb = function hue2rgb(p, q, t){
      if(t < 0) t += 1;
      if(t > 1) t -= 1;
      if(t < 1/6) return p + (q - p) * 6 * t;
      if(t < 1/2) return q;
      if(t < 2/3) return p + (q - p) * (2/3 - t) * 6;
      return p;
    }
    var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    var p = 2 * l - q;
    r = hue2rgb(p, q, h + 1/3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1/3);
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

function readMap(map) {
  gMap = map;
  computeDistance(map);
  if (gMap)
    updateView(gMap, gMoves || [], 0);
}

function readMoves(moves) {
  gMoves = moves.moves;
  if (gMap)
    updateView(gMap, gMoves || [], 0);
}

function updateView(map, moves, step) {
  var sites = map.sites || [];
  var rivers = map.rivers || [];
  var mines = map.mines || [];

  var minX, maxX, minY, maxY;
  minX = minY = INF;
  maxX = maxY = -INF;
  for (var i = 0; i < sites.length; i++) {
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
  var scaleX = document.documentElement.clientWidth * 0.9 / (maxX - minX);
  var scaleY = document.documentElement.clientHeight * 0.8 / (maxY - minY);
  var scale = Math.min(scaleX, scaleY);
  canvas.width = (maxX - minX) * scale;
  canvas.height = (maxY - minY) * scale;

  var sitePX = new Array(sites.length);
  var sitePY = new Array(sites.length);
  for (var i = 0; i < sites.length; i++) {
    sitePX[i] = convertX(sites[i].x, minX, maxX, canvas.width);
    sitePY[i] = convertY(sites[i].y, minY, maxY, canvas.height);
  }
  var isMine = new Array(sites.length);
  for (var i = 0; i < mines.length; i++) {
    isMine[mines[i]] = true;
  }

  // Assign colors to players
  var players = new Set();
  for (var i = 0; i < moves.length; i++) {
    players.add(moves[i].claim.punter);
  }
  var colors = new Array(players.size);
  for (var i = 0; i < colors.length; i++) {
    var hue = (0.4 + i / colors.length) / 1.0;
    var rgb = hslToRgb(hue, 1.0, 0.5);
    colors[i] = 'rgb(' + rgb[0] + ',' + rgb[1] + ',' + rgb[2] + ')';
  }

  var ctx = canvas.getContext("2d");

  // Draw rivers
  ctx.lineWidth = 1;
  for (var i = 0; i < rivers.length; i++) {
    ctx.beginPath();
    ctx.strokeStyle = 'rgb(120, 120, 120)';
    var src = rivers[i].source;
    var tgt = rivers[i].target;
    ctx.moveTo(sitePX[src], sitePY[src]);
    ctx.lineTo(sitePX[tgt], sitePY[tgt]);
    ctx.stroke();
  }

  // Draw claimed rivers
  for (var i = 0; i < step; i++) {
    var claim = gMoves[i].claim;
    ctx.beginPath();

    ctx.strokeStyle = colors[claim.punter];
    ctx.moveTo(sitePX[claim.source], sitePY[claim.source]);
    ctx.lineTo(sitePX[claim.target], sitePY[claim.target]);
    ctx.lineWidth = i == step-1 ? 5 : 2;
    ctx.stroke();
  }

  // Draw sites
  for (var i =0; i < sitePX.length; i++) {
    ctx.beginPath();
    ctx.arc(sitePX[i], sitePY[i], isMine[i] ? 6 : 3, 2 * Math.PI, false);
    ctx.fillStyle = isMine[i] ? 'red' : 'gray';
    ctx.fill();
  }

  // Update buttons
  btnPrev.disabled = !(step > 0);
  btnNext.disabled = !(step < moves.length);

  // Update score

  // edges[p][i]: punter p's i-th edge.
  var edges = new Array(players.size);
  for (var i = 0; i < edges.length; i++)
    edges[i] = [];
  for (var i = 0; i < step; i++) {
    var punter = gMoves[i].claim.punter;
    var src = gMoves[i].claim.source;
    var dst = gMoves[i].claim.target;
    edges[punter].push({src: src, dst: dst});
  }

  var scores = new Array(edges.length);
  for (var p = 0; p < edges.length; p++) {
    // Collect adjacent nodes.
    var punter = p; 

    scores[p] = 0;
    for (var i = 0; i < mines.length; i++)
      scores[p] += computeScore(i, edges, punter, sites.length, mines);
    console.log('Punter ' + p + ' = ' + scores[p]);
  }
  scoreContainer.textContent = '';
  for (var i = 0; i < scores.length; i++) {
    var el = document.createElement('span');
    el.textContent = scores[i];
    el.style.color = colors[i];
    scoreContainer.appendChild(el);
  }
}

function computeScoreRec(v, mineIndex, adj, depth, visited, mines) {
  if (visited[v])
    return 0;
  
  visited[v] = true;
  score = gDistance[mineIndex][v] * gDistance[mineIndex][v];
  for (var i = 0; i < adj[v].length; i++) {
    var v2 = adj[v][i];
    score += computeScoreRec(v2, mineIndex, adj, depth, visited, mines);
  }
  return score;
}

function computeScore(mineIndex, edges, punter, V, mines) {
  var visited = new Array(V);
  var adj = new Array(V);
  for (var i = 0; i < adj.length; i++)
    adj[i] = [];

  for (var i = 0; i < edges[punter].length; i++) {
    var src = edges[punter][i].src;
    var dst = edges[punter][i].dst;
    adj[src].push(dst);
    adj[dst].push(src);
  }
  return computeScoreRec(mines[mineIndex], mineIndex, adj, 0, visited, mines);
}

function computeDistance(map) {
  var sites = map.sites || [];
  var rivers = map.rivers || [];
  var mines = map.mines || [];

  var adj = new Array(sites.length);
  for (var i = 0; i < adj.length; i++)
    adj[i] = [];
  for (var i = 0; i < rivers.length; i++) {
    var src = rivers[i].source;
    var tgt = rivers[i].target;
    adj[src].push(tgt);
    adj[tgt].push(src);
  }

  gDistance = new Array(mines.length);
  for (var i = 0; i < mines.length; i++) {
    gDistance[i] = new Array(sites.length);
    for (var j = 0; j < sites.length; j++)
      gDistance[i][j] = INF;
    
    var visited = new Array(sites.length);
    var S = mines[i];
    var queue = [];
    visited[S] = true;
    gDistance[i][S] = 0;
    queue.push({v: S, d: 0});
    
    while (queue.length != 0) {
      var v = queue[0].v;
      var d = queue[0].d;
      queue.shift();
      for (var j = 0; j < adj[v].length; j++) {
        var v2 = adj[v][j];
        if (!visited[v2]) {
          visited[v2] = true;
          gDistance[i][v2] = d + 1;
          queue.push({v: v2, d: d+1});
        }
      }
    }
  }
}

function onPrev() {
  gStep = Math.max(gStep - 1, 0);
  updateView(gMap, gMoves, gStep);
}

function onNext() {
  gStep = Math.min(gStep + 1, gMoves.length);
  updateView(gMap, gMoves, gStep);
}

document.addEventListener('DOMContentLoaded', function(event) {
  wrapper = document.getElementById('wrapper');
  canvas = document.getElementById('canvas');
  scoreContainer = document.getElementById('scores');
  btnPrev = document.getElementById('prev');
  btnNext = document.getElementById('next');

  var inputMap = document.getElementById('map');
  inputMap.addEventListener('change', function(event) {
    var reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      var map = JSON.parse(event.target.result);
      readMap(map);
    };
  });

  var inputMoves = document.getElementById('moves');
  inputMoves.addEventListener('change', function(event) {
    var reader = new FileReader();
    reader.readAsText(event.target.files[0]);
    reader.onload = function(event) {
      var moves = JSON.parse(event.target.result);
      readMoves(moves);
    };
  });

  btnPrev.addEventListener('click', onPrev);
  btnNext.addEventListener('click', onNext);
});
