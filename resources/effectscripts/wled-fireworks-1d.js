/*
  QLC+ Effect Script: WLED Fireworks 1D
  Rockets shoot up from the start of the strip, burst at a random height into sparks
  that spread and fade; intensity is how often.
  Written from what WLED's "Fireworks 1D" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fireworks 1D";
    effect.description = "From WLED: rockets that rise and burst into sparks";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fireworks 1D";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var cycle = WLED.period(seg.intensity, 4000, 900);      // a rocket every so often
        seg.fill(seg.color(1));
        for (var r = 0; r < 3; r++) {
            var t0 = seg.now - r * cycle;
            var n = Math.floor(t0 / (3 * cycle));
            var t = (t0 % (3 * cycle) + 3 * cycle) % (3 * cycle);
            var height = (0.45 + 0.45 * WLED.hash(n, r)) * (len - 1);
            var rise = WLED.period(seg.speed, 1500, 400);
            var col = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(n, r + 3) * 255), false, true, 0)
                                  : WLED.hue(Math.floor(WLED.hash(n, r + 3) * 256));
            if (t < rise) {
                var p = Math.round(height * Math.sin(t / rise * Math.PI / 2));
                seg.setPixelColor(p, 0xFFF0C0);
            } else {
                var age = (t - rise) / 1500;
                if (age < 1) {
                    var spread = age * len / 5;
                    for (var k = 0; k < 8; k++) {
                        var sp = Math.round(height + (WLED.hash(n, 20 + k) * 2 - 1) * spread);
                        seg.setPixelColor(sp, WLED.colorBlend(0, col, Math.round((1 - age) * 255)));
                    }
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
