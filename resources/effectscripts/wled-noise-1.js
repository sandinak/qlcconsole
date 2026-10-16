/*
  QLC+ Effect Script: WLED Noise 1
  Fast-shifting noise: wide patches of the palette (or wheel) sliding and changing
  quickly.
  Written from what WLED's "Noise 1" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noise 1";
    effect.description = "From WLED: fast-shifting noise";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noise 1";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 6000, 300);
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(i * 0.12 + s, 141);
            seg.setPixelColor(i, seg.randomColor((n * 1.4 + s * 0.05) % 1));
        }
        return seg.out();
    };

    return effect;
})();
