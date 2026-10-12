/*
  QLC+ Effect Script: WLED Candle Multi
  Like Candle, but every pixel is its own candle flickering on its own.
  Written from what WLED's "Candle Multi" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Candle Multi";
    effect.description = "From WLED: a candle in every pixel";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Candle Multi";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var rate = WLED.period(seg.speed, 500, 60);
        var depth = 0.2 + seg.intensity / 255 * 0.7;
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(seg.now / rate + i * 7.3, 110 + i);
            seg.setPixelColor(i, WLED.colorBlend(0, seg.palette ? paint(i) : (seg.color(0) || 0xFF8020), Math.round((1 - depth * n) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
