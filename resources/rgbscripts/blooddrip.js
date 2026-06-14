/*
  Q Light Controller Plus
  blooddrip.js

  Horror/theatrical effect: a stab wound drips blood downward into a
  spreading pool at the bottom of the matrix.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 1;
  algo.name = "Blood Drip";
  algo.author = "Augment";
  algo.acceptColors = 0;

  algo.properties = [];

  // Stage convention: X 0=stage left, 100=stage right, 50=center
  //                   Y 0=upstage (top of matrix), 100=downstage
  // Blood drips downstage (toward audience) and pools at the downstage edge.
  algo.woundX = 50;
  algo.properties.push("name:woundX|type:range|display:Stage Pos L-R (%)|values:0,100|write:setWoundX|read:getWoundX");
  algo.woundY = 5;
  algo.properties.push("name:woundY|type:range|display:Stage Depth 0=Upstage (%)|values:0,70|write:setWoundY|read:getWoundY");
  algo.woundWidth = 20;
  algo.properties.push("name:woundWidth|type:range|display:Wound Width (%)|values:5,60|write:setWoundWidth|read:getWoundWidth");
  algo.dripCount = 5;
  algo.properties.push("name:dripCount|type:range|display:Max Drips|values:1,20|write:setDripCount|read:getDripCount");
  algo.dripSpeed = 3;
  algo.properties.push("name:dripSpeed|type:range|display:Drip Speed|values:1,10|write:setDripSpeed|read:getDripSpeed");
  algo.poolSpread = 3;
  algo.properties.push("name:poolSpread|type:range|display:Pool Spread|values:0,10|write:setPoolSpread|read:getPoolSpread");

  algo.setWoundX = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.woundX=n; };
  algo.getWoundX = function(){ return algo.woundX; };
  algo.setWoundY = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=70) algo.woundY=n; };
  algo.getWoundY = function(){ return algo.woundY; };
  algo.setWoundWidth = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=5&&n<=60) algo.woundWidth=n; };
  algo.getWoundWidth = function(){ return algo.woundWidth; };
  algo.setDripCount = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=20) algo.dripCount=n; };
  algo.getDripCount = function(){ return algo.dripCount; };
  algo.setDripSpeed = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=10) algo.dripSpeed=n; };
  algo.getDripSpeed = function(){ return algo.dripSpeed; };
  algo.setPoolSpread = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=10) algo.poolSpread=n; };
  algo.getPoolSpread = function(){ return algo.poolSpread; };

  var util = {};
  util.initialized = false;
  util.drips = [];
  util.stain = null;  // [y][x] = 0..255, dried blood
  util.pool = null;   // [x] = height from bottom in rows
  util.phase = 0;
  util.w = 0; util.h = 0;

  function clamp(v, lo, hi){ return v < lo ? lo : (v > hi ? hi : v); }

  function init(w, h) {
    util.w = w; util.h = h;
    util.drips = [];
    util.stain = [];
    for (var y = 0; y < h; y++) {
      util.stain[y] = [];
      for (var x = 0; x < w; x++) util.stain[y][x] = 0;
    }
    util.pool = [];
    for (var x2 = 0; x2 < w; x2++) util.pool[x2] = 0;
    util.phase = 0;
    util.initialized = true;
  }

  function spawnDrip(w, h) {
    var cx = Math.floor(w * algo.woundX / 100.0);
    var halfW = Math.max(1, Math.floor(w * algo.woundWidth / 200.0));
    var x = cx + Math.floor((Math.random() * 2 - 1) * halfW);
    x = clamp(x, 0, w - 1);
    var startY = Math.floor(h * algo.woundY / 100.0);
    util.drips.push({
      x: x,
      yf: startY,
      speed: (0.3 + Math.random() * 0.7) * (algo.dripSpeed / 5.0),
      bright: 0.75 + Math.random() * 0.25,
      wobbleTimer: Math.floor(Math.random() * 12),
      wobbleDir: 0
    });
  }

  algo.rgbMap = function(width, height, _rgb, _step) {
    if (!util.initialized || util.w !== width || util.h !== height) {
      init(width, height);
    }
    util.phase++;

    var woundRow = Math.floor(height * algo.woundY / 100.0);

    // Spawn drips
    if (util.drips.length < algo.dripCount) {
      var spawnRate = 0.08 + 0.04 * algo.dripSpeed;
      if (Math.random() < spawnRate) spawnDrip(width, height);
    }

    // Update drips
    var alive = [];
    for (var d = 0; d < util.drips.length; d++) {
      var drip = util.drips[d];
      var yi = Math.floor(drip.yf);

      // Leave stain at current pixel
      if (yi >= 0 && yi < height) {
        var cur = util.stain[yi][drip.x];
        util.stain[yi][drip.x] = Math.min(255, cur + 80);
      }

      // Occasional horizontal wobble (blood follows body contours)
      drip.wobbleTimer++;
      if (drip.wobbleTimer > 10 + Math.floor(Math.random() * 10)) {
        drip.wobbleTimer = 0;
        var r = Math.random();
        drip.wobbleDir = (r < 0.25) ? -1 : (r < 0.5) ? 1 : 0;
      }
      drip.x = clamp(drip.x + drip.wobbleDir, 0, width - 1);
      drip.yf += drip.speed;

      // Check collision with pool or bottom
      var yi2 = Math.floor(drip.yf);
      var poolTop = height - 1 - util.pool[drip.x];
      if (yi2 >= poolTop) {
        // Grow pool at landing column + splash neighbors
        var maxPoolH = height - woundRow - 2;
        if (util.pool[drip.x] < maxPoolH) util.pool[drip.x]++;
        if (drip.x > 0 && util.pool[drip.x - 1] < util.pool[drip.x] - 1)
          util.pool[drip.x - 1]++;
        if (drip.x < width - 1 && util.pool[drip.x + 1] < util.pool[drip.x] - 1)
          util.pool[drip.x + 1]++;
        // Stain the surface
        if (poolTop >= 0 && poolTop < height)
          util.stain[poolTop][drip.x] = 255;
        // Drip is absorbed
      } else {
        alive.push(drip);
      }
    }
    util.drips = alive;

    // Fade stains slowly (dried blood darkens but doesn't vanish)
    if (util.phase % 5 === 0) {
      for (var sy = 0; sy < height; sy++) {
        for (var sx = 0; sx < width; sx++) {
          var s = util.stain[sy][sx];
          if (s > 45) util.stain[sy][sx] = s - 3;
        }
      }
    }

    // Pool spread sideways
    if (algo.poolSpread > 0 && util.phase % Math.max(1, Math.floor(12 / algo.poolSpread)) === 0) {
      var newPool = util.pool.slice();
      for (var px = 0; px < width; px++) {
        var maxN = 0;
        if (px > 0) maxN = Math.max(maxN, util.pool[px - 1]);
        if (px < width - 1) maxN = Math.max(maxN, util.pool[px + 1]);
        if (maxN > util.pool[px] + 1) newPool[px] = util.pool[px] + 1;
      }
      util.pool = newPool;
    }

    // Build output map
    var map = [];
    for (var y = 0; y < height; y++) {
      map[y] = new Array(width);
      for (var x = 0; x < width; x++) map[y][x] = 0;
    }

    // Draw stains (dried blood)
    for (var sy2 = 0; sy2 < height; sy2++) {
      for (var sx2 = 0; sx2 < width; sx2++) {
        var sv = util.stain[sy2][sx2];
        if (sv > 0) {
          var t = sv / 255.0;
          var r = Math.floor(0x20 + 0x60 * t);
          map[sy2][sx2] = (r << 16);
        }
      }
    }

    // Draw pool (dark blood from the bottom up)
    for (var px2 = 0; px2 < width; px2++) {
      var ph = util.pool[px2];
      for (var py = 0; py < ph; py++) {
        var row = height - 1 - py;
        if (row < 0 || row >= height) continue;
        if (py === ph - 1) {
          // Surface: slightly brighter with shimmer
          var shimmer = 0.7 + 0.3 * Math.sin(util.phase * 0.18 + px2 * 0.6);
          var pr = Math.floor(0x99 * shimmer);
          map[row][px2] = (clamp(pr, 0, 255) << 16);
        } else {
          // Depth: darkens away from surface
          var depth = (ph - 1 - py) / Math.max(1, ph);
          var pr2 = Math.floor(0x38 + 0x28 * (1.0 - depth));
          map[row][px2] = (clamp(pr2, 0, 255) << 16);
        }
      }
    }

    // Draw active drips (trail + bright head)
    for (var d2 = 0; d2 < util.drips.length; d2++) {
      var drip2 = util.drips[d2];
      var headY = Math.floor(drip2.yf);
      var trailLen = 4;

      // Fading trail above head
      for (var t = 1; t <= trailLen; t++) {
        var ty = headY - t;
        if (ty < 0 || ty >= height) continue;
        var ti = (1.0 - t / (trailLen + 1)) * drip2.bright;
        var tr = Math.floor(0xCC * ti);
        if (tr > 0) {
          var existing = map[ty][drip2.x];
          var er = (existing >> 16) & 0xFF;
          if (tr > er) map[ty][drip2.x] = (tr << 16);
        }
      }
      // Bright head
      if (headY >= 0 && headY < height) {
        var hr = Math.floor(0xFF * drip2.bright);
        var hg = Math.floor(0x14 * drip2.bright);
        map[headY][drip2.x] = (clamp(hr, 0, 255) << 16) + (clamp(hg, 0, 255) << 8);
      }
    }

    // Draw wound (pulsing red spot)
    var wCX = Math.floor(width * algo.woundX / 100.0);
    var wHalfW = Math.max(1, Math.floor(width * algo.woundWidth / 200.0));
    var pulse = 0.55 + 0.45 * Math.sin(util.phase * 0.22);
    for (var wx = -wHalfW; wx <= wHalfW; wx++) {
      var wxi = wCX + wx;
      if (wxi < 0 || wxi >= width) continue;
      var edgeFade = 1.0 - Math.abs(wx) / (wHalfW + 1);
      var wr = Math.floor((0xBB + 0x44 * pulse) * edgeFade);
      var wg = Math.floor(0x0A * pulse * edgeFade);
      if (woundRow >= 0 && woundRow < height)
        map[woundRow][wxi] = (clamp(wr, 0, 255) << 16) + (clamp(wg, 0, 255) << 8);
      if (woundRow + 1 < height) {
        var below = Math.floor(wr * 0.45);
        if ((below << 16) > map[woundRow + 1][wxi])
          map[woundRow + 1][wxi] = (clamp(below, 0, 255) << 16);
      }
    }

    return map;
  };

  algo.rgbMapStepCount = function(_w, _h){ return 1; };

  return algo;
})();
