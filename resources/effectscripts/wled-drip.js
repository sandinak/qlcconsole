/*
  QLC+ Effect Script: WLED Drip
  Drops gather at the far end of the strip, swell, fall to the start and splash;
  intensity is how many drops at once.
  Written from what WLED's "Drip" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Drip";
    effect.description = "From WLED: drops forming, falling and splashing";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Drip";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = 1 + Math.round(seg.intensity / 255 * 3);
        var cycle = WLED.period(seg.speed, 5000, 1200);
        seg.fill(seg.color(1));
        var col = seg.palette ? paint(0) : (seg.color(0) || 0x0060FF);
        for (var d = 0; d < count; d++) {
            var t = ((seg.now + d * cycle / count) % cycle) / cycle;
            if (t < 0.4) {                                      // swelling at the top
                seg.setPixelColor(len - 1, WLED.colorBlend(0, col, Math.round(t / 0.4 * 255)));
            } else if (t < 0.75) {                              // falling, faster and faster
                var f = (t - 0.4) / 0.35;
                seg.setPixelColor(Math.round((len - 1) * (1 - f * f)), col);
            } else {                                            // the splash
                var a = 1 - (t - 0.75) / 0.25;
                var w = Math.round((1 - a) * 3);
                for (var k = -w; k <= w; k++)
                    if (k !== 0 || a > 0.7)
                        seg.setPixelColor(Math.abs(k), WLED.colorBlend(0, col, Math.round(a * 200)));
            }
        }
        return seg.out();
    };

    return effect;
})();
