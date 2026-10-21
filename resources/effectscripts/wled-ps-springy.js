/*
  QLC+ Effect Script: WLED PS Springy
  A row of particles joined by springs: every few seconds a push at the start
  sends a wave of squeezing along the row that dies away; speed is how stiff the
  springs are, intensity how many particles.
  Written from what WLED's "PS Springy" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Springy";
    effect.description = "From WLED: particles on springs, a wave running through them";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Springy";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = Math.max(2, Math.round(len / (2 + (255 - seg.intensity) / 255 * 4)));
        var sp = len / count;
        var c = count / (WLED.period(seg.speed, 4000, 600) / 1000);    // particles a second the wave runs
        var every = 3000;
        var tau = (seg.now % every) / 1000;
        seg.fill(seg.color(1));
        for (var k = 0; k < count; k++) {
            var front = c * tau - k;
            var d = sp * 0.8 * Math.exp(-tau * 0.8) * Math.exp(-front * front / 6) * Math.sin(front * 1.2);
            var x = (k + 0.5) * sp + d;
            var i = Math.floor(x), w = x - i;
            var col = seg.randomColor(k / count);
            seg.setPixelColor(i, WLED.add(seg.getPixelColor(i), WLED.dim(col, 1 - w)));
            seg.setPixelColor(i + 1, WLED.add(seg.getPixelColor(i + 1), WLED.dim(col, w)));
        }
        return seg.out();
    };

    return effect;
})();
