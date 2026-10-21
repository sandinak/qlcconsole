/*
  QLC+ Effect Script: WLED Noise 4
  Noise sparkle: every pixel flickering on its own in brightness and colour, a
  glittering field of the palette (or wheel).
  Written from what WLED's "Noise 4" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noise 4";
    effect.description = "From WLED: noise sparkle";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noise 4";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 4000, 200);
        for (var i = 0; i < len; i++) {
            var b = WLED.noise(s * 4 + i * 7.31, 146);
            var n = WLED.noise(i * 0.08 + s * 0.2, 147);
            seg.setPixelColor(i, WLED.dim(seg.randomColor(n), b * b * b));
        }
        return seg.out();
    };

    return effect;
})();
