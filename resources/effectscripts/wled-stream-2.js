/*
  QLC+ Effect Script: WLED Stream 2
  Random hues flowing smoothly along the strip, one melting into the next.
  Written from what WLED's "Stream 2" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Stream 2";
    effect.description = "From WLED: random hues flowing along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Stream 2";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 300, 15);           // ms a pixel
        var s = seg.now / period;
        for (var i = 0; i < len; i++) {
            var p = (i - s) / 6;
            var k = Math.floor(p), f = p - k;
            f = f * f * (3 - 2 * f);
            var u = WLED.hash(k, 71), v = WLED.hash(k + 1, 71);
            seg.setPixelColor(i, seg.randomColor(u + (v - u) * f));
        }
        return seg.out();
    };

    return effect;
})();
