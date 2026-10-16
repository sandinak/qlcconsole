/*
  QLC+ Effect Script: WLED Noise 3
  Finer, slower noise: narrow bands of the palette (or wheel), their brightness
  from the same noise.
  Written from what WLED's "Noise 3" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noise 3";
    effect.description = "From WLED: fine shifting noise";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noise 3";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 10000, 600);
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(i * 0.3 + s * 0.5, 144);
            var b = WLED.noise(i * 0.3 + s * 0.5 + 0.5, 145);
            seg.setPixelColor(i, WLED.dim(seg.randomColor((n * 1.6 + s * 0.03) % 1), 0.3 + 0.7 * b));
        }
        return seg.out();
    };

    return effect;
})();
