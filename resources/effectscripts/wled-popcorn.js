/*
  QLC+ Effect Script: WLED Popcorn
  Kernels pop up from the start of the strip and fall back, each a random colour;
  intensity is how many.
  Written from what WLED's "Popcorn" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Popcorn";
    effect.description = "From WLED: kernels popping up and falling back";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Popcorn";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = 1 + Math.round(seg.intensity / 255 * 8);
        var air = WLED.period(seg.speed, 2500, 600);
        seg.fill(seg.color(1));
        for (var k = 0; k < count; k++) {
            var period = air * (1.4 + WLED.hash(k, 1));
            var t = (seg.now + WLED.hash(k, 2) * period) % period;
            var n = Math.floor((seg.now + WLED.hash(k, 2) * period) / period);
            if (t > air) continue;                              // waiting to pop again
            var top = (0.3 + 0.7 * WLED.hash(n, k)) * (len - 1);
            var u = t / air;
            var p = Math.round(top * 4 * u * (1 - u));          // up and back down
            seg.setPixelColor(p, seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(n, k + 5) * 255), false, true, 0)
                                             : WLED.hue(Math.floor(WLED.hash(n, k + 5) * 256)));
        }
        return seg.out();
    };

    return effect;
})();
