/*
  Q Light Controller Plus
  fountainbasin.js

  Fountain basin: caustic light patterns and slow basin slosh —
  simulates submerged LED wash or light refracting through
  moving water onto basin walls/floor.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 3;
  algo.name = "Fountain Basin";
  algo.author = "Augment";
  algo.acceptColors = 3; // dark / mid / bright
  algo.properties = new Array();

  algo.speed = 12;
  algo.properties.push("name:speed|type:range|display:Speed|values:1,50|write:setSpeed|read:getSpeed");
  algo.caustics = 65;
  algo.properties.push("name:caustics|type:range|display:Caustics|values:0,100|write:setCaustics|read:getCaustics");
  algo.slosh = 40;
  algo.properties.push("name:slosh|type:range|display:Slosh|values:0,100|write:setSlosh|read:getSlosh");
  algo.scale = 12;
  algo.properties.push("name:scale|type:range|display:Scale|values:4,40|write:setScale|read:getScale");

  var util = { t: 0, initialized: false, colors: [] };

  algo.setSpeed    = function(v){ algo.speed    = parseInt(v); };
  algo.getSpeed    = function(){ return algo.speed; };
  algo.setCaustics = function(v){ algo.caustics = parseInt(v); };
  algo.getCaustics = function(){ return algo.caustics; };
  algo.setSlosh    = function(v){ algo.slosh    = parseInt(v); };
  algo.getSlosh    = function(){ return algo.slosh; };
  algo.setScale    = function(v){ algo.scale    = parseInt(v); };
  algo.getScale    = function(){ return algo.scale; };

  function defaultPalette(){ return [0x000D1A, 0x005577, 0xAAEEFF]; }

  algo.rgbMapSetColors = function(rawColors){
    if (!Array.isArray(rawColors)) return;
    util.colors = [];
    for (var i = 0; i < algo.acceptColors; i++){
      if (i < rawColors.length && rawColors[i] === rawColors[i]) util.colors.push(rawColors[i]);
    }
    if (util.colors.length === 0) util.colors = defaultPalette();
  };

  function getPalette(){ return util.colors.length ? util.colors : defaultPalette(); }

  function lerpColor(a, b, t){
    var ar=(a>>16)&255,ag=(a>>8)&255,ab=a&255;
    var br=(b>>16)&255,bg=(b>>8)&255,bb=b&255;
    return(Math.floor(ar+(br-ar)*t)<<16)+(Math.floor(ag+(bg-ag)*t)<<8)+Math.floor(ab+(bb-ab)*t);
  }

  function samplePalette(pal, idx){
    if (pal.length===1) return pal[0];
    var x=idx*(pal.length-1),i=Math.floor(x),j=Math.min(pal.length-1,i+1);
    return lerpColor(pal[i],pal[j],x-i);
  }

  algo.rgbMap = function(width, height, _rgb, _step){
    if (!util.initialized){ util.t = 0; util.initialized = true; }
    var sp   = algo.speed / 50.0;
    var camt = algo.caustics / 100.0;
    var samt = algo.slosh / 100.0;
    var sc   = algo.scale / 10.0;
    var t    = util.t;
    util.t  += 0.028 * sp;

    var pal = getPalette();
    var map = new Array(height);

    for (var y = 0; y < height; y++){
      map[y] = new Array(width);
      for (var x = 0; x < width; x++){
        var nx = width  > 1 ? x / (width  - 1) : 0.5;
        var ny = height > 1 ? y / (height - 1) : 0.5;

        // Slow basin-wide drift (gravity-driven slosh, very low frequency)
        var dx = samt * 0.12 * Math.sin(t * 0.28 + 0.5);
        var dy = samt * 0.09 * Math.cos(t * 0.21);
        var px = nx + dx;
        var py = ny + dy;

        // Layer 1: basin resonance — standing wave (low frequency, slow)
        var slosh = Math.sin(px*sc*2.9 + t*0.9) * Math.cos(py*sc*2.4 - t*0.75) * 0.5 + 0.5;

        // Layer 2: mid-frequency ripple interference
        var r1 = Math.sin(px*sc*6.1 - py*sc*3.7 + t*2.1);
        var r2 = Math.sin(px*sc*4.8 + py*sc*6.9 - t*2.9 + 1.618);
        var ripple = (r1 + r2) * 0.25 + 0.5;

        // Layer 3: caustic shimmer — product of two high-freq waves
        // (product creates narrow bright flecks where both waves peak simultaneously)
        var c1 = Math.sin(px*sc*16.7 + py*sc*11.3 - t*5.4) * 0.5 + 0.5;
        var c2 = Math.sin(py*sc*13.9 - px*sc*9.1 + t*3.9 + 2.718) * 0.5 + 0.5;
        var c3 = Math.sin((px+py)*sc*8.3 - t*4.1 + 1.414) * 0.5 + 0.5;
        var caustic = c1 * c2 * c3;
        caustic = caustic * caustic * camt;    // cube → sharp bright points

        var v = slosh * (0.35 + samt*0.15)
              + ripple * 0.30
              + caustic * 0.45;
        v = Math.max(0, Math.min(1, v));

        var color = samplePalette(pal, v);

        // Strong caustics bloom toward white
        if (caustic > 0.35){
          color = lerpColor(color, 0xFFFFFF, Math.min(1, (caustic - 0.35) * camt * 2.5));
        }

        map[y][x] = color;
      }
    }
    return map;
  };

  algo.rgbMapStepCount = function(_w, _h){ return 2; };

  return algo;
})();
