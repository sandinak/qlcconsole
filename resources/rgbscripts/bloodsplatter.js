/*
  Q Light Controller Plus
  bloodsplatter.js

  Horror/theatrical effect: periodic blood splatter impacts shoot
  particles outward from a point, leaving spreading stains.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 1;
  algo.name = "Blood Splatter";
  algo.author = "Augment";
  algo.acceptColors = 0;

  algo.properties = [];

  // Stage convention: X 0=stage left, 100=stage right, 50=center
  //                   Y 0=upstage (top of matrix), 100=downstage
  // Splatter radiates from the impact point; gravity carries particles downstage.
  algo.centerX = 50;
  algo.properties.push("name:centerX|type:range|display:Stage Pos L-R (%)|values:0,100|write:setCenterX|read:getCenterX");
  algo.centerY = 10;
  algo.properties.push("name:centerY|type:range|display:Stage Depth 0=Upstage (%)|values:0,100|write:setCenterY|read:getCenterY");
  algo.wander = 30;
  algo.properties.push("name:wander|type:range|display:Position Wander (%)|values:0,100|write:setWander|read:getWander");
  algo.burstSize = 12;
  algo.properties.push("name:burstSize|type:range|display:Particles Per Burst|values:3,30|write:setBurstSize|read:getBurstSize");
  algo.burstSpeed = 5;
  algo.properties.push("name:burstSpeed|type:range|display:Burst Speed|values:1,10|write:setBurstSpeed|read:getBurstSpeed");
  algo.burstInterval = 50;
  algo.properties.push("name:burstInterval|type:range|display:Burst Interval (steps)|values:10,200|write:setBurstInterval|read:getBurstInterval");
  algo.stainLife = 5;
  algo.properties.push("name:stainLife|type:range|display:Stain Life|values:1,10|write:setStainLife|read:getStainLife");

  algo.setCenterX = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.centerX=n; };
  algo.getCenterX = function(){ return algo.centerX; };
  algo.setCenterY = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.centerY=n; };
  algo.getCenterY = function(){ return algo.centerY; };
  algo.setWander = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.wander=n; };
  algo.getWander = function(){ return algo.wander; };
  algo.setBurstSize = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=3&&n<=30) algo.burstSize=n; };
  algo.getBurstSize = function(){ return algo.burstSize; };
  algo.setBurstSpeed = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=10) algo.burstSpeed=n; };
  algo.getBurstSpeed = function(){ return algo.burstSpeed; };
  algo.setBurstInterval = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=10&&n<=200) algo.burstInterval=n; };
  algo.getBurstInterval = function(){ return algo.burstInterval; };
  algo.setStainLife = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=10) algo.stainLife=n; };
  algo.getStainLife = function(){ return algo.stainLife; };

  var util = {};
  util.initialized = false;
  util.particles = [];
  util.stain = null;  // [y][x] = 0..255
  util.phase = 0;
  util.nextBurst = 5;
  util.burstCX = 0.5; util.burstCY = 0.5;
  util.w = 0; util.h = 0;

  function clamp(v, lo, hi){ return v < lo ? lo : (v > hi ? hi : v); }

  function init(w, h) {
    util.w = w; util.h = h;
    util.particles = [];
    util.stain = [];
    for (var y = 0; y < h; y++) {
      util.stain[y] = [];
      for (var x = 0; x < w; x++) util.stain[y][x] = 0;
    }
    util.phase = 0;
    util.nextBurst = 5;
    util.initialized = true;
  }

  function doBurst(w, h) {
    var wanderR = algo.wander / 100.0 * 0.5;
    var bx = algo.centerX / 100.0 + (Math.random() * 2 - 1) * wanderR;
    var by = algo.centerY / 100.0 + (Math.random() * 2 - 1) * wanderR;
    util.burstCX = clamp(bx, 0, 1);
    util.burstCY = clamp(by, 0, 1);

    var cx = util.burstCX * (w - 1);
    var cy = util.burstCY * (h - 1);
    // Scale speed relative to matrix size so effect looks similar at any resolution
    var scale = Math.max(w, h) / 16.0;
    var speedBase = (algo.burstSpeed / 5.0) * scale;

    var N = algo.burstSize;
    for (var i = 0; i < N; i++) {
      // Evenly spread angles with jitter
      var angle = (Math.PI * 2 * i / N) + (Math.random() - 0.5) * (Math.PI * 2 / N);
      var speed = (0.4 + Math.random() * 1.2) * speedBase;
      util.particles.push({
        x: cx, y: cy,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 1.0,
        decay: 0.07 + Math.random() * 0.06,
        bright: 0.8 + Math.random() * 0.2
      });
    }
    // Extra downstage drips (blood carried toward audience by gravity after impact)
    var extras = Math.max(1, Math.floor(N * 0.40));
    for (var j = 0; j < extras; j++) {
      // π/2 = straight downstage; spread ±50° so particles fan toward audience
      var dAngle = Math.PI * 0.5 + (Math.random() - 0.5) * Math.PI * 0.55;
      var dSpeed = (0.3 + Math.random() * 0.8) * speedBase;
      util.particles.push({
        x: cx + (Math.random() - 0.5) * scale,
        y: cy,
        vx: Math.cos(dAngle) * dSpeed,
        vy: Math.sin(dAngle) * dSpeed,
        life: 1.0,
        decay: 0.04 + Math.random() * 0.04,
        bright: 0.7 + Math.random() * 0.3
      });
    }
  }

  algo.rgbMap = function(width, height, _rgb, _step) {
    if (!util.initialized || util.w !== width || util.h !== height) {
      init(width, height);
    }
    util.phase++;

    // Trigger new burst
    if (util.phase >= util.nextBurst) {
      doBurst(width, height);
      util.nextBurst = util.phase + algo.burstInterval;
    }

    // Update particles
    var alive = [];
    for (var d = 0; d < util.particles.length; d++) {
      var p = util.particles[d];

      // Stain pixels along the path
      var xi = Math.round(p.x); var yi = Math.round(p.y);
      if (xi >= 0 && xi < width && yi >= 0 && yi < height) {
        var add = Math.floor(180 * p.life * p.bright);
        util.stain[yi][xi] = Math.min(255, util.stain[yi][xi] + add);
      }

      // Move particle
      p.x += p.vx; p.y += p.vy;
      // Gravity
      p.vy += 0.05;
      // Friction
      p.vx *= 0.87; p.vy *= 0.87;
      // Age
      p.life -= p.decay;

      if (p.life > 0 && p.x > -2 && p.x < width + 2 && p.y > -2 && p.y < height + 2) {
        alive.push(p);
      } else {
        // Final stain at resting spot
        var lxi = clamp(Math.round(p.x), 0, width - 1);
        var lyi = clamp(Math.round(p.y), 0, height - 1);
        util.stain[lyi][lxi] = 255;
      }
    }
    util.particles = alive;

    // Fade stains (higher stainLife = slower fade)
    if (util.phase % 4 === 0) {
      var fadeAmt = Math.max(1, 11 - algo.stainLife);
      for (var sy = 0; sy < height; sy++) {
        for (var sx = 0; sx < width; sx++) {
          var s = util.stain[sy][sx];
          if (s > 35) util.stain[sy][sx] = Math.max(35, s - fadeAmt);
          else if (s > 0) util.stain[sy][sx] = Math.max(0, s - 1);
        }
      }
    }

    // Build output map
    var map = [];
    for (var y = 0; y < height; y++) {
      map[y] = new Array(width);
      for (var x = 0; x < width; x++) map[y][x] = 0;
    }

    // Draw stains
    for (var sy2 = 0; sy2 < height; sy2++) {
      for (var sx2 = 0; sx2 < width; sx2++) {
        var sv = util.stain[sy2][sx2];
        if (sv > 0) {
          var t = sv / 255.0;
          // Fresh: bright red; dry: near-black dark red
          var r = Math.floor(0x18 + 0x77 * t);
          map[sy2][sx2] = (r << 16);
        }
      }
    }

    // Draw active particles (trail + head)
    for (var d2 = 0; d2 < util.particles.length; d2++) {
      var p2 = util.particles[d2];

      // Trail pixel (one step behind)
      var tx = Math.round(p2.x - p2.vx);
      var ty = Math.round(p2.y - p2.vy);
      if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
        var tr = Math.floor(0xAA * p2.life * p2.bright);
        if ((tr << 16) > map[ty][tx]) map[ty][tx] = (clamp(tr, 0, 255) << 16);
      }

      // Head (brightest)
      var hx = Math.round(p2.x); var hy = Math.round(p2.y);
      if (hx >= 0 && hx < width && hy >= 0 && hy < height) {
        var hr = Math.floor(0xFF * p2.life * p2.bright);
        var hg = Math.floor(0x18 * p2.life * p2.bright);
        var hpix = (clamp(hr, 0, 255) << 16) + (clamp(hg, 0, 255) << 8);
        if (hpix > map[hy][hx]) map[hy][hx] = hpix;
      }
    }

    // Draw impact point (pulsing red core)
    var icx = Math.round(util.burstCX * (width - 1));
    var icy = Math.round(util.burstCY * (height - 1));
    if (icx >= 0 && icx < width && icy >= 0 && icy < height) {
      var pulse = 0.5 + 0.5 * Math.sin(util.phase * 0.2);
      var wr = Math.floor(0xFF * pulse);
      var wg = Math.floor(0x12 * pulse);
      map[icy][icx] = (clamp(wr, 0, 255) << 16) + (clamp(wg, 0, 255) << 8);
    }

    return map;
  };

  algo.rgbMapStepCount = function(_w, _h){ return 1; };

  return algo;
})();
