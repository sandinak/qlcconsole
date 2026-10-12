/*
  QLC+ Effect Script: WLED Fill Noise
  The palette (or wheel) painted by smooth noise that drifts along, wide patches of
  colour flowing into each other.
  Written from what WLED's "Fill Noise" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fill Noise";
    effect.description = "From WLED: smooth noise flowing through colour";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fill Noise";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 15000, 1000);
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(i * 0.06 + s, 120);
            var idx = Math.floor(n * 255 + s * 30) & 255;
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(idx, false, true, 0) : WLED.hue(idx));
        }
        return seg.out();
    };

    return effect;
})();
