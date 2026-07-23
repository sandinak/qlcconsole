/*
  Q Light Controller Plus
  fountainlip.js

  LED strip along a fountain rim — simulates the water sheet
  going over the lip: slow curtain fingers, cresting waves, foam.

  Licensed under the Apache License, Version 2.0
*/

(function(){
  var algo = {};
  algo.apiVersion = 3;
  algo.name = "Fountain Lip";
  algo.author = "Augment";
  algo.acceptColors = 3; // deep / flow / foam
  algo.properties = new Array();

  algo.speed = 18;
  algo.properties.push("name:speed|type:range|display:Speed|values:1,50|write:setSpeed|read:getSpeed");
  algo.foam = 50;
  algo.properties.push("name:foam|type:range|display:Foam|values:0,100|write:setFoam|read:getFoam");
  algo.crestWidth = 25;
  algo.properties.push("name:crestWidth|type:range|display:Crest Width|values:5,60|write:setCrestWidth|read:getCrestWidth");
  algo.scale = 8;
  algo.properties.push("name:scale|type:range|display:Scale|values:1,30|write:setScale|read:getScale");

  var util = { t: 0, initialized: false, colors: [] };

  algo.setSpeed      = function(v){ algo.speed      = parseInt(v); };
  algo.getSpeed      = function(){ return algo.speed; };
  algo.setFoam       = function(v){ algo.foam       = parseInt(v); };
  algo.getFoam       = function(){ return algo.foam; };
  algo.setCrestWidth = function(v){ algo.crestWidth = parseInt(v); };
  algo.getCrestWidth = function(){ return algo.crestWidth; };
  algo.setScale      = function(v){ algo.scale      = parseInt(v); };
  algo.getScale      = function(){ return algo.scale; };

  function defaultPalette(){ return [0x001A2E, 0x0077BB, 0xDDF4FF]; }

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
    var sp  = algo.speed / 50.0;
    var sc  = algo.scale / 10.0;
    var fa  = algo.foam / 100.0;
    var cw  = 0.05 + (algo.crestWidth / 100.0) * 0.45;
    var t   = util.t;
    util.t += 0.038 * sp;

    var pal = getPalette();
    var map = new Array(height);

    for (var y = 0; y < height; y++){
      map[y] = new Array(width);
      for (var x = 0; x < width; x++){
        var pos = width > 1 ? x / (width - 1) : 0.5;

        // Curtain: nearly static positions where water bunches
        // (these drift very slowly — the sheet oscillates side-to-side)
        var curtain = Math.sin(pos*sc*4.0 - t*0.4) * 0.5;

        // Surface waves traveling along the rim from the fountain center
        var tw1 = Math.sin(pos*sc*7.2 - t*3.2 + curtain);
        var tw2 = Math.sin(pos*sc*4.9 + t*2.1 + 1.618);
        var tw3 = Math.sin(pos*sc*11.3 - t*4.8 + 3.141);

        // Weighted sum: curtain dominates at low speed, waves at high
        var wave = curtain*0.45 + tw1*0.30 + tw2*0.15 + tw3*0.10;
        // wave is roughly -1..1

        var v = wave * 0.42 + 0.52;   // remap → 0.1..0.94

        // Foam: sharp brightening above crest threshold
        var foam = 0.0;
        var thresh = 1.0 - cw;
        if (fa > 0 && wave > thresh){
          foam = Math.pow((wave - thresh) / cw, 2.0) * fa;
        }

        var palIdx = Math.min(1.0, v * 0.88 + foam * 0.55);
        var color  = samplePalette(pal, palIdx);

        // Strong foam → push toward white
        if (foam > 0.3){
          color = lerpColor(color, 0xFFFFFF, Math.min(1, (foam - 0.3) * fa * 2.2));
        }

        map[y][x] = color;
      }
    }
    return map;
  };

  algo.rgbMapStepCount = function(_w, _h){ return 2; };

  return algo;
})();
