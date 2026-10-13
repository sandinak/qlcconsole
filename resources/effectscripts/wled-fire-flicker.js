/*
  QLC+ Effect Script: WLED Fire Flicker
  Colour 1 flickering like a flame -- the whole strip together, each pixel a little
  different; intensity is how hard it flickers.
  Written from what WLED's "Fire Flicker" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fire Flicker";
    effect.description = "From WLED: colour flickering like a flame";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fire Flicker";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var rate = WLED.period(seg.speed, 300, 30);
        var depth = 0.15 + seg.intensity / 255 * 0.8;
        var t = seg.now / rate;
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(t + i * 0.37, 60 + (i % 7));
            seg.setPixelColor(i, WLED.colorBlend(0, paint(i), Math.round((1 - depth * n) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
