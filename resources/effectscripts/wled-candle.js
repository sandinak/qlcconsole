/*
  QLC+ Effect Script: WLED Candle
  The whole strip flickers in colour 1 like a candle flame; intensity is how deep the
  flicker goes.
  Written from what WLED's "Candle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Candle";
    effect.description = "From WLED: a candle's flicker";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Candle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var rate = WLED.period(255 - seg.speed, 60, 400);
        var t = seg.now / rate;
        var depth = 0.15 + seg.intensity / 255 * 0.75;
        var lvl = 1 - depth * (0.6 * WLED.noise(t, 1) + 0.4 * WLED.noise(t * 2.7, 2));
        var a = Math.round(Math.max(0, Math.min(1, lvl)) * 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), a));
        return seg.out();
    };

    return effect;
})();
