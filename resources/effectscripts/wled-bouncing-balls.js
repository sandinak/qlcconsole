/*
  QLC+ Effect Script: WLED Bouncing Balls
  Balls dropped from the top of the strip bounce at its start, each bounce lower, then
  are dropped again; intensity is how many balls.
  Written from what WLED's "Bouncing Balls" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Bouncing Balls";
    effect.description = "From WLED: balls bouncing under gravity";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Bouncing Balls";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var count = 1 + Math.round(seg.intensity / 255 * 6);
        var drop = WLED.period(255 - seg.speed, 2500, 9000);    // until a ball is dropped again
        seg.fill(seg.color(1));
        for (var b = 0; b < count; b++) {
            var t = ((seg.now + b * drop / count) % drop) / 1000;
            // Fall from the top, bounce with 70% of the speed each time.
            var g = 2.0, h0 = 1,          // a first drop of about a second
                v = Math.sqrt(2 * g * h0), tt = t, h = 0;
            var fall = Math.sqrt(2 * h0 / g);
            if (tt < fall) h = h0 - 0.5 * g * tt * tt;
            else {
                tt -= fall;
                while (v > 0.05) {
                    var up = 2 * v / g;
                    if (tt < up) { h = v * tt - 0.5 * g * tt * tt; break; }
                    tt -= up; v *= 0.7;
                }
            }
            var i = Math.round(Math.max(0, h) / h0 * (len - 1));
            var col = seg.palette ? seg.colorFromPalette(b * 255 / count, false, true, 0)
                                  : (b % 3 === 0 ? seg.color(0) : b % 3 === 1 ? (seg.color(2) || seg.color(0)) : seg.color(0));
            seg.setPixelColor(i, col);
        }
        return seg.out();
    };

    return effect;
})();
