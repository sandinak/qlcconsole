/*
  Q Light Controller Plus
  ribbons.js

  Chaotic ribbons/attractor with soft glow and evolving parameters.
  - Draws a de Jong-style attractor as flowing ribbons
  - Additive glow with trail fading across frames
  - Parameters drift toward random targets, causing the pattern to evolve

  Options
  - Color (acceptColors=1)
  - Fading (%)
  - Speed (1..10)
  - Size (% of matrix)

  QtScript-friendly (no ES6), API v3 for color.
  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 3;
  algo.name = "Ribbons";
  algo.author = "Augment";
  algo.acceptColors = 1; // one color used for ribbons
  algo.properties = [];

  // --- Properties ---
  algo.fadePct = 35; // 0..100: higher = shorter trails
  algo.properties.push("name:fade|type:range|display:Fading (%)|values:0,100|write:setFade|read:getFade");
  algo.setFade = function(v){ var n=parseInt(v,10); if (isNaN(n)) n=35; if(n<0)n=0; if(n>100)n=100; algo.fadePct=n; };
  algo.getFade = function(){ return algo.fadePct; };

  algo.speed = 6; // 1..10, controls steps per frame and parameter drift rate
  algo.properties.push("name:speed|type:range|display:Speed|values:1,10|write:setSpeed|read:getSpeed");
  algo.setSpeed = function(v){ var n=parseInt(v,10); if(isNaN(n)) n=6; if(n<1)n=1; if(n>10)n=10; algo.speed=n; };
  algo.getSpeed = function(){ return algo.speed; };

  algo.sizePct = 85; // 10..100 % of min(width,height)
  algo.properties.push("name:size|type:range|display:Size (% of matrix)|values:10,100|write:setSize|read:getSize");
  algo.setSize = function(v){ var n=parseInt(v,10); if(isNaN(n)) n=85; if(n<10)n=10; if(n>100)n=100; algo.sizePct=n; };
  algo.getSize = function(){ return algo.sizePct; };

  // --- Color handling (API v3) ---
  var util = {};
  util.color = 0x13FFD6; // teal default
  algo.rgbMapSetColors = function(raw){ if (!raw || !raw.length) return; util.color = raw[0]; };
  algo.rgbMapGetColors = function(){ return [util.color]; };

  // Additional property: Density (%) controls how many strokes per frame
  algo.densityPct = 120; // 10..300
  algo.properties.push("name:density|type:range|display:Density (%)|values:10,300|write:setDensity|read:getDensity");
  algo.setDensity = function(v){ var n=parseInt(v,10); if(isNaN(n)) n=120; if(n<10)n=10; if(n>300)n=300; algo.densityPct=n; };
  algo.getDensity = function(){ return algo.densityPct; };

  // Minimum occupancy to avoid collapsing too small
  algo.minOccPct = 50; // 0..100
  algo.properties.push("name:minocc|type:range|display:Min Occupancy (%)|values:0,100|write:setMinOcc|read:getMinOcc");
  algo.setMinOcc = function(v){ var n=parseInt(v,10); if(isNaN(n)) n=50; if(n<0)n=0; if(n>100)n=100; algo.minOccPct=n; };
  algo.getMinOcc = function(){ return algo.minOccPct; };


  // --- State ---
  util.buf = [];      // intensity buffer 0..255 per pixel (glow/trail)
  util.lastW = 0; util.lastH = 0; util.frame = 0;
  util.meanX = 0.0; util.meanY = 0.0; // recentering mean in attractor space
  util.scaleAdjX = 1.0; util.scaleAdjY = 1.0; // auto-fit scaling (<=1), never upscales

  // Attractor state and evolution
  util.x = 0.1; util.y = 0.0;
  util.a = 1.40; util.b = -2.30; util.c = 2.40; util.d = -2.10; // pleasant defaults
  util.ta = 1.40; util.tb = -2.30; util.tc = 2.40; util.td = -2.10; // targets
  util.ticksToTarget = 600; util.ticksLeft = 600;

  // --- Helpers ---
  function ensureBuf(w,h){
    if (util.lastW!==w || util.lastH!==h){
      util.buf = new Array(h);
      var y; for (y=0;y<h;y++){ util.buf[y] = new Array(w); var x; for (x=0;x<w;x++) util.buf[y][x]=0; }
      util.lastW=w; util.lastH=h; util.frame=0; util.x=0.1; util.y=0.0;
      util.meanX=0.0; util.meanY=0.0; util.scaleAdjX=1.0; util.scaleAdjY=1.0;
      util.ticksToTarget = 600; util.ticksLeft = util.ticksToTarget;
    }
  }

  function clamp(v,a,b){ return v<a?a:(v>b?b:v); }

  function decayBuf(){
    // Map fading% to per-frame decay factor: 0% -> 1.0 (no fade), 100% -> ~0.1
    var fac = 1.0 - (algo.fadePct/100.0)*0.9; if (fac < 0.1) fac = 0.1; if (fac > 1.0) fac = 1.0;
    var h = util.lastH, w = util.lastW; var y,x;
    for (y=0;y<h;y++){
      var row = util.buf[y];
      for (x=0;x<w;x++){ var v = row[x]*fac; row[x] = v<1?0:v; }
    }
  }

  function addToBuf(x, y, v){
    var w=util.lastW, h=util.lastH; var xi=Math.floor(x), yi=Math.floor(y);
    if (xi<0||xi>=w||yi<0||yi>=h) return;
    var nv = util.buf[yi][xi] + v; if (nv>255) nv=255; util.buf[yi][xi]=nv;
  }

  function brushAt(x, y, radius, amp){
    // Soft circular brush; amp ~ 0..1, radius in px
    var w=util.lastW, h=util.lastH;
    var xi=Math.floor(x), yi=Math.floor(y);
    if (radius < 1) radius = 1;
    var r2 = radius*radius;
    var dy, dx;
    for (dy=-radius; dy<=radius; dy++){
      var yy = yi+dy; if (yy<0||yy>=h) continue;
      for (dx=-radius; dx<=radius; dx++){
        var xx = xi+dx; if (xx<0||xx>=w) continue;
        var d2 = dx*dx + dy*dy; if (d2>r2) continue;
        var wgt = amp * (1 - d2/(r2+1)); // radial falloff
        var nv = util.buf[yy][xx] + wgt*10.0; // scale into 0..255 space
        if (nv>255) nv=255; util.buf[yy][xx]=nv;
      }
    }
  }

  function nextDeJong(x,y,a,b,c,d){
    // de Jong attractor: x' = sin(a*y) - cos(b*x); y' = sin(c*x) - cos(d*y)
    var nx = Math.sin(a*y) - Math.cos(b*x);
    var ny = Math.sin(c*x) - Math.cos(d*y);
    return {x:nx, y:ny};
  }

  function evolveParams(){
    if (util.ticksLeft <= 0){
      // new random targets in a pleasant range [-3..3]
      util.ta = (Math.random()*6 - 3);
      util.tb = (Math.random()*6 - 3);
      util.tc = (Math.random()*6 - 3);
      util.td = (Math.random()*6 - 3);
      // duration scales with speed and increases evolution when large
      var base = 600; var speedScale = (11 - algo.speed)/10.0;
      var occ = util.occLarge; if (!occ || occ<0) occ=0; if (occ>1) occ=1;
      var largeAdj = 1.0 - 0.6 * Math.max(0, occ - 0.5); // 1.0..0.7 as occ 0.5..1.0
      util.ticksToTarget = Math.max(150, Math.floor(base * speedScale * largeAdj));
      util.ticksLeft = util.ticksToTarget;
    }
    // drift toward target, faster when occupancy is large
    var lerp = 1.0 / (util.ticksToTarget>0?util.ticksToTarget:1);
    var occ2 = util.occLarge; if (!occ2 || occ2<0) occ2=0; if (occ2>1) occ2=1;
    var boost = 1.0 + 0.8 * Math.max(0, occ2 - 0.6);
    var le = lerp * boost;
    util.a += (util.ta - util.a) * le; util.b += (util.tb - util.b) * le;
    util.c += (util.tc - util.c) * le; util.d += (util.td - util.d) * le;
    util.ticksLeft -= 1;
  }

  function renderToBuf(w,h){
    // Steps are driven by Density (%); Speed controls evolution rate only
    var steps = 80 + Math.floor( (Math.min(w,h) * algo.densityPct) * 0.05 );
    if (steps < 60) steps = 60; if (steps > 4000) steps = 4000;
    var sXBase = (w * 0.5) * (algo.sizePct/100.0) * 0.95;
    var sYBase = (h * 0.5) * (algo.sizePct/100.0) * 0.95;
    var sX = sXBase * util.scaleAdjX;
    var sY = sYBase * util.scaleAdjY;
    var cx = (w-1)*0.5, cy = (h-1)*0.5;

    var baseR = Math.floor(Math.min(w,h) * 0.06); if (baseR<1) baseR=1; if (baseR>8) baseR=8;

    var i; var px = util.x, py = util.y; // previous point
    var sumx=0.0, sumy=0.0, cnt=0; var maxAbsX=0.0, maxAbsY=0.0;
    for (i=0;i<steps;i++){
      var np = nextDeJong(px, py, util.a, util.b, util.c, util.d);
      var u = (i+1)/steps; // 0..1 recentness within this frame
      var dx = np.x - util.meanX, dy = np.y - util.meanY;
      var x1 = cx + dx*sX, y1 = cy + dy*sY;
      var R = Math.floor(baseR * (0.6 + 0.6*u)); if (R<1) R=1;
      var amp = 0.3 + 0.7*u; // heavier near the head
      brushAt(x1,y1,R,amp);
      px = np.x; py = np.y;
      sumx += np.x; sumy += np.y; cnt++;
      var adx = Math.abs(dx), ady = Math.abs(dy);
      if (adx>maxAbsX) maxAbsX=adx;
      if (ady>maxAbsY) maxAbsY=ady;
    }
    util.x = px; util.y = py;

    // Update mean center slowly to keep content centered over time
    if (cnt>0){
      var avgx = sumx/cnt, avgy = sumy/cnt;
      var smooth = 0.08; // smoothing factor
      util.meanX = util.meanX*(1.0-smooth) + avgx*smooth;
      util.meanY = util.meanY*(1.0-smooth) + avgy*smooth;
    }

    // Auto-fit per axis with minimum occupancy: grow if too small, shrink if too large
    var halfW = w*0.48, halfH = h*0.48; // leave margin
    var occMin = (algo.minOccPct/100.0);

    if (maxAbsX>0){
      var projXBase = sXBase * maxAbsX;
      var projXCur  = projXBase * util.scaleAdjX;
      if (projXCur > halfW){
        var needX = halfW / (projXBase>0?projXBase:1);
        util.scaleAdjX = util.scaleAdjX*0.9 + needX*0.1;
      } else if (projXCur < halfW*occMin){
        var growX = (halfW*occMin) / (projXBase>0?projXBase:1);
        if (growX > util.scaleAdjX){ util.scaleAdjX = util.scaleAdjX*0.9 + growX*0.1; }
        else { util.scaleAdjX = util.scaleAdjX*0.98 + 1.0*0.02; }
      } else {
        // relax toward 1.0 when comfortably within bounds
        util.scaleAdjX = util.scaleAdjX*0.98 + 1.0*0.02;
      }
      if (util.scaleAdjX<0.1) util.scaleAdjX=0.1; if (util.scaleAdjX>3.0) util.scaleAdjX=3.0;
    }

    if (maxAbsY>0){
      var projYBase = sYBase * maxAbsY;
      var projYCur  = projYBase * util.scaleAdjY;
      if (projYCur > halfH){
        var needY = halfH / (projYBase>0?projYBase:1);
        util.scaleAdjY = util.scaleAdjY*0.9 + needY*0.1;
      } else if (projYCur < halfH*occMin){
        var growY = (halfH*occMin) / (projYBase>0?projYBase:1);
        if (growY > util.scaleAdjY){ util.scaleAdjY = util.scaleAdjY*0.9 + growY*0.1; }
        else { util.scaleAdjY = util.scaleAdjY*0.98 + 1.0*0.02; }
      } else {
        util.scaleAdjY = util.scaleAdjY*0.98 + 1.0*0.02;
      }
      if (util.scaleAdjY<0.1) util.scaleAdjY=0.1; if (util.scaleAdjY>3.0) util.scaleAdjY=3.0;
    }

    // record occupancy for evolution logic
    var occX = 0.0, occY = 0.0;
    if (maxAbsX>0){ var projXCur2 = sXBase*maxAbsX*util.scaleAdjX; occX = projXCur2/(halfW>0?halfW:1); }
    if (maxAbsY>0){ var projYCur2 = sYBase*maxAbsY*util.scaleAdjY; occY = projYCur2/(halfH>0?halfH:1); }
    util.occLarge = (occX>occY?occX:occY); if (util.occLarge>1.0) util.occLarge=1.0; if (util.occLarge<0) util.occLarge=0;
  }

  function colorFromIntensity(inten, rgb){
    var r=(rgb>>16)&255, g=(rgb>>8)&255, b=rgb&255; var a = inten/255.0; if (a<0) a=0; if (a>1) a=1;
    var rr = Math.floor(r*a), gg = Math.floor(g*a), bb = Math.floor(b*a);
    return (rr<<16)|(gg<<8)|bb;
  }

  function toMap(){
    var h=util.lastH, w=util.lastW; var map=new Array(h); var y,x;
    var col = util.color;
    for (y=0;y<h;y++){
      var row = new Array(w); map[y]=row; var src = util.buf[y];
      for (x=0;x<w;x++){ row[x] = colorFromIntensity(src[x], col); }
    }
    return map;
  }

  // API entry points
  algo.rgbMap = function(width, height, _rgb, _step){
    ensureBuf(width,height);
    decayBuf();
    evolveParams();
    renderToBuf(width,height);
    util.frame += 1;
    return toMap();
  };

  algo.rgbMapStepCount = function(_w,_h){ return 32767; };

  return algo;
})();

