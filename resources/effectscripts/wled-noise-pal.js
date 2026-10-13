/*
  QLC+ Effect Script: WLED Noise Pal
  The palette (or colour wheel) painted by slowly drifting smooth noise: patches of
  colour that wander and morph.
  Written from what WLED's "Noise Pal" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noise Pal";
    effect.description = "From WLED: patches of palette colour drifting";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noise Pal";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 20000, 1500);
        var scale = 0.05 + seg.intensity / 255 * 0.4;
        for (var i = 0; i < len; i++) {
            var n = WLED.noise(i * scale + s, 13) * 0.7 + WLED.noise(i * scale * 2.3 - s * 0.6, 17) * 0.3;
            var idx = Math.floor(n * 255) & 0xFF;
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(idx, false, true, 0) : WLED.hue(idx));
        }
        return seg.out();
    };

    return effect;
})();
