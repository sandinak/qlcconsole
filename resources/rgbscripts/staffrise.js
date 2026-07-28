/*
  Q Light Controller Plus
  staffrise.js

  Staff LED strip: magical energy surges from the ground end to the tip.
  Cycle: dark silence → comet rises with glowing trail → full sparkle hold → fade → repeat.

  Fixture group: vertical strip, bottom row = ground/source, top row = tip.
  Use Direction property if your group is oriented the other way.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 2;
  algo.name = "Staff Rise";
  algo.author = "Branson/Augment";
  algo.acceptColors = 1;
  algo.properties = new Array();

  algo.speed = 5;
  algo.properties.push("name:speed|type:range|display:Speed|values:1,10|write:setSpeed|read:getSpeed");

  algo.trailPct = 35;
  algo.properties.push("name:trailPct|type:range|display:Trail Length %|values:5,90|write:setTrailPct|read:getTrailPct");

  algo.orientation = 0; // 0 = bottom→top, 1 = top→bottom
  algo.properties.push("name:orientation|type:list|display:Direction|values:Bottom to Top,Top to Bottom|write:setOrientation|read:getOrientation");

  algo.setSpeed = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=10) algo.speed=n; };
  algo.getSpeed = function(){ return algo.speed; };
  algo.setTrailPct = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=5&&n<=90) algo.trailPct=n; };
  algo.getTrailPct = function(){ return algo.trailPct; };
  algo.setOrientation = function(v){ algo.orientation = (v === "Top to Bottom") ? 1 : 0; };
  algo.getOrientation = function(){ return (algo.orientation === 1) ? "Top to Bottom" : "Bottom to Top"; };

  var TWO_PI = 6.2831853;

  // Cycle fractions
  var RISE_END = 0.38;  // rise phase: 0..RISE_END
  var HOLD_END = 0.65;  // sparkle hold: RISE_END..HOLD_END
                        // fade: HOLD_END..1.0

  var util = {};
  util.cyclePhase = 0.0;
  util.shimClock  = 0.0;
  util.shimPh     = null; // per-pixel sparkle offsets
  util.pixCount   = 0;

  function clamp(v,lo,hi){ return v<lo?lo:v>hi?hi:v; }

  function lerpColor(a,b,t){
    if(t<=0) return a; if(t>=1) return b;
    var ar=(a>>16)&255, ag=(a>>8)&255, ab=a&255;
    var br=(b>>16)&255, bg=(b>>8)&255, bb=b&255;
    return ((Math.floor(ar+(br-ar)*t))<<16)|
           ((Math.floor(ag+(bg-ag)*t))<<8)|
            Math.floor(ab+(bb-ab)*t);
  }

  function scaleBrightness(c,k){
    if(k<=0) return 0; if(k>=1) return c;
    var r=(c>>16)&255, g=(c>>8)&255, b=c&255;
    return ((Math.floor(r*k))<<16)|((Math.floor(g*k))<<8)|Math.floor(b*k);
  }

  algo.rgbMap = function(width, height, rgb, step){
    var total = width * height;

    if(!util.shimPh || util.pixCount !== total){
      util.shimPh = new Array(total);
      for(var i=0; i<total; i++) util.shimPh[i] = Math.random() * TWO_PI;
      util.pixCount   = total;
      util.cyclePhase = 0.0;
      util.shimClock  = 0.0;
    }

    util.cyclePhase += 0.0022 * algo.speed;
    if(util.cyclePhase >= 1.0) util.cyclePhase -= 1.0;
    util.shimClock += 0.08;
    if(util.shimClock > 1e5) util.shimClock = 0.0;

    var cp       = util.cyclePhase;
    var tipColor = lerpColor(rgb, 0xFFFFFF, 0.80);
    var trailMax = algo.trailPct / 100.0;

    var map = new Array(height);
    var idx = 0;

    for(var y=0; y<height; y++){
      map[y] = new Array(width);
      for(var x=0; x<width; x++){
        var ph  = util.shimPh[idx];
        var col = 0x000000;

        // normY: 0 = source (ground end), 1 = tip
        var normY;
        if(algo.orientation === 0){
          normY = (height > 1) ? (height-1-y) / (height-1) : 0.5;
        } else {
          normY = (height > 1) ? y / (height-1) : 0.5;
        }

        if(cp < RISE_END){
          // Comet rising: wave front moves from normY=0 → normY=1
          var risePct     = cp / RISE_END;             // 0..1
          var distBehind  = risePct - normY;           // +ve = behind front (filled)

          if(normY > risePct + 0.015){
            col = 0x000000; // ahead of wave, unlit
          } else if(distBehind <= 0.015){
            col = tipColor; // at the front: hot tip
          } else if(distBehind < trailMax){
            var t = distBehind / trailMax;
            col = scaleBrightness(rgb, Math.exp(-t * 3.2));
          }
          // else: deep behind trail — dark
        } else if(cp < HOLD_END){
          // Full glow with twinkle shimmer
          var shim = 0.65 + 0.35 * Math.sin(util.shimClock * 1.3 + ph)
                          * Math.sin(util.shimClock * 0.7 - ph * 0.5);
          col = scaleBrightness(lerpColor(rgb, tipColor, 0.40), 0.55 + 0.45 * shim);
        } else {
          // Fade out
          var fadePct = (cp - HOLD_END) / (1.0 - HOLD_END);
          var fadeK   = Math.pow(1.0 - fadePct, 2.0);
          var shim2   = 0.75 + 0.25 * Math.sin(util.shimClock + ph);
          col = scaleBrightness(lerpColor(rgb, tipColor, 0.25), shim2 * fadeK * 0.8);
        }

        map[y][x] = col;
        idx++;
      }
    }

    return map;
  };

  algo.rgbMapStepCount = function(width, height){ return 128; };

  return algo;
})();
