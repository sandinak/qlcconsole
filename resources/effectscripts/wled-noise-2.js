/*
  QLC+ Effect Script: WLED Noise 2
  Fast-shifting noise in colour and in brightness: bright patches of the palette
  (or wheel) drifting over darker ones.
  Written from what WLED's "Noise 2" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noise 2";
    effect.description = "From WLED: fast-shifting noise, bright and dark";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noise 2";
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
            var n = WLED.noise(i * 0.1 + s, 142);
            var b = WLED.noise(i * 0.2 - s * 0.7, 143);
            seg.setPixelColor(i, WLED.dim(seg.randomColor((n * 1.4) % 1), 0.15 + 0.85 * b * b));
        }
        return seg.out();
    };

    return effect;
})();
