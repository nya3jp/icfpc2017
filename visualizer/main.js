const INF = Math.pow(10, 18);
var wrapper;
var canvas;
var btnPrev;
var btnNext;
var gMap;
var gMoves;
var gStep = 0;

function convertX(x, minX, maxX, canvasWidth) {
  return (x - minX) * (canvasWidth * 0.9) / (maxX - minX) + canvasWidth * 0.05;
}

function convertY(y, minY, maxY, canvasHeight) {
  return (y - minY) * (canvasHeight * 0.9) / (maxY - minY) + canvasHeight * 0.05;
}

function hslToRgb(h, s, l){
    var r, g, b;

    if(s == 0){
        r = g = b = l; // achromatic
    }else{
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
  console.log(map);
  if (gMap)
    updateView(gMap, gMoves || [], 0);
}

function readMoves(moves) {
  gMoves = moves.moves;
  console.log(moves);
  if (gMap)
    updateView(gMap, gMoves || [], 0);
}

function updateView(map, moves, step) {
  var sites = map.sites;
  var rivers = map.rivers;
  var mines = map.mines;

  var minX, maxX, minY, maxY;
  minX = minY = INF;
  maxX = maxY = -INF;
  for (var i = 0; i < sites.length; i++) {
    console.log(sites[i].x, sites[i].y);
    minX = Math.min(minX, sites[i].x);
    maxX = Math.max(maxX, sites[i].x);
    minY = Math.min(minY, sites[i].y);
    maxY = Math.max(maxY, sites[i].y);
  }
  maxX = Math.max(maxX, minX + 1);
  maxY = Math.max(maxY, minY + 1);
  var w = maxX - minX;
  var h = maxY - minY;

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
  ctx.lineWidth = 3;
  for (var i = 0; i < step; i++) {
    var claim = gMoves[i].claim;
    ctx.beginPath();

    ctx.strokeStyle = colors[claim.punter];
    ctx.moveTo(sitePX[claim.source], sitePY[claim.source]);
    ctx.lineTo(sitePX[claim.target], sitePY[claim.target]);
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
  console.log(step, moves.length);
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
