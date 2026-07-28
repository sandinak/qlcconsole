/*
  Q Light Controller Plus
  staffuplights.js

  Uplight cascade: a burst of light radiates from the staff impact point,
  firing each uplight as the wavefront reaches it. Uplights hold a glow
  after being hit, then decay between pulses.

  Impact X/Y set the origin as a percentage of the fixture group size.
  Pulse Rate controls how often a new burst fires.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 2;
  algo.name = "Staff Uplights";
  algo.author = "Branson/Augment";
  algo.acceptColors = 1;
  algo.properties = new Array();

  algo.speed = 6;
  algo.properties.push("name:speed|type:range|display:Wave Speed|values:1,20|write:setSpeed|read:getSpeed");

  algo.pulseRate = 80; // frames between pulses
  algo.properties.push("name:pulseRate|type:range|display:Pulse Rate (frames)|values:20,200|write:setPulseRate|read:getPulseRate");

  algo.glowFloor = 25; // % sustained glow after hit
  algo.properties.push("name:glowFloor|type:range|display:Glow Floor %|values:0,80|write:setGlowFloor|read:getGlowFloor");

  algo.decay = 12; // fade speed between pulses (higher = faster)
  algo.properties.push("name:decay|type:range|display:Decay|values:1,50|write:setDecay|read:getDecay");

  algo.impactX = 50;
  algo.properties.push("name:impactX|type:range|display:Impact X %|values:0,100|write:setImpactX|read:getImpactX");

  algo.impactY = 50;
  algo.properties.push("name:impactY|type:range|display:Impact Y %|values:0,100|write:setImpactY|read:getImpactY");

  algo.setSpeed     = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=20)  algo.speed=n; };
  algo.getSpeed     = function(){ return algo.speed; };
  algo.setPulseRate = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=20&&n<=200) algo.pulseRate=n; };
  algo.getPulseRate = function(){ return algo.pulseRate; };
  algo.setGlowFloor = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=80)  algo.glowFloor=n; };
  algo.getGlowFloor = function(){ return algo.glowFloor; };
  algo.setDecay     = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=1&&n<=50)  algo.decay=n; };
  algo.getDecay     = function(){ return algo.decay; };
  algo.setImpactX   = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.impactX=n; };
  algo.getImpactX   = function(){ return algo.impactX; };
  algo.setImpactY   = function(v){ var n=parseInt(v,10); if(!isNaN(n)&&n>=0&&n<=100) algo.impactY=n; };
  algo.getImpactY   = function(){ return algo.impactY; };

  var util = {};
  util.initialized = false;
  util.brt         = null;  // [y][x] float 0..1, persistent brightness
  util.frame       = 0;
  util.pulseR      = 0;     // current pulse wavefront radius
  util.prevPulseR  = 0;
  util.speedAcc    = 0.0;
  util.w           = 0;
  util.h           = 0;

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
    if(!util.initialized || util.w !== width || util.h !== height){
      util.brt = new Array(height);
      for(var y=0; y<height; y++){
        util.brt[y] = new Array(width);
        for(var x=0; x<width; x++) util.brt[y][x] = 0.0;
      }
      util.w = width; util.h = height;
      util.frame      = 0;
      util.pulseR     = 0;
      util.prevPulseR = 0;
      util.speedAcc   = 0.0;
      util.initialized = true;
    }

    util.frame++;

    // Fire a new pulse periodically
    if(util.frame % algo.pulseRate === 0){
      util.pulseR     = 0;
      util.prevPulseR = 0;
      util.speedAcc   = 0.0;
    }

    var cx = (width  > 1) ? (width-1)  * (algo.impactX / 100.0) : 0;
    var cy = (height > 1) ? (height-1) * (algo.impactY / 100.0) : 0;
    var maxD = Math.sqrt(width*width + height*height) + 2;

    // Advance wavefront and fire pixels in the swept annulus
    if(util.pulseR <= maxD){
      util.speedAcc += algo.speed / 8.0;
      var steps = Math.floor(util.speedAcc);
      util.speedAcc -= steps;

      if(steps > 0){
        var rInner = util.prevPulseR;
        util.pulseR     += steps;
        util.prevPulseR  = util.pulseR;

        for(var y=0; y<height; y++){
          for(var x=0; x<width; x++){
            var dx = x-cx, dy = y-cy;
            var d = Math.sqrt(dx*dx+dy*dy);
            if(d >= rInner && d < util.pulseR){
              util.brt[y][x] = 1.0; // fire this uplight
            }
          }
        }
      }
    }

    // Decay all brightness toward glow floor, then output color
    var floor     = algo.glowFloor / 100.0;
    var decayRate = algo.decay / 800.0;
    var tipColor  = lerpColor(rgb, 0xFFFFFF, 0.55);

    var map = new Array(height);
    for(var y=0; y<height; y++){
      map[y] = new Array(width);
      for(var x=0; x<width; x++){
        var b = util.brt[y][x];
        if(b > floor) b = Math.max(floor, b - decayRate);
        util.brt[y][x] = b;

        var col;
        if(b >= 0.85){
          col = lerpColor(rgb, tipColor, (b - 0.85) / 0.15);
        } else {
          col = scaleBrightness(rgb, b / 0.85);
        }
        map[y][x] = col;
      }
    }

    return map;
  };

  algo.rgbMapStepCount = function(width, height){ return 256; };

  return algo;
})();
