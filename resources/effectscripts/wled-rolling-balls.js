/*
  QLC+ Effect Script: WLED Rolling Balls
  Balls of different colours rolling along the strip at their own speeds, bouncing
  off its ends.
  Written from what WLED's "Rolling Balls" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Rolling Balls";
    effect.description = "From WLED: balls rolling and bouncing off the ends";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Rolling Balls";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var balls = 1 + Math.round(seg.intensity / 255 * 7);
        var base = WLED.period(seg.speed, 8000, 800);
        seg.fill(seg.color(1));
        for (var b = 0; b < balls; b++) {
            var period = base * (0.6 + 0.9 * WLED.hash(b, 130));
            var t = ((seg.now / period + WLED.hash(b, 131)) % 2 + 2) % 2;
            var p = Math.round((t < 1 ? t : 2 - t) * (len - 1));
            seg.setPixelColor(p, seg.palette ? seg.colorFromPalette(Math.floor(b / balls * 255), false, true, 0) : WLED.hue(Math.floor(b * 256 / balls)));
        }
        return seg.out();
    };

    return effect;
})();
