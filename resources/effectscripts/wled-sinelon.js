/*
  QLC+ Effect Script: WLED Sinelon
  A dot swings back and forth on a smooth sine, leaving a fading trail; its colour comes
  from the palette where it is.
  Written from what WLED's "Sinelon" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sinelon";
    effect.description = "From WLED: a swinging dot with a fading trail";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sinelon";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 500);
        var tail = 1 + seg.intensity / 255 * len / 3;
        var pos = (0.5 - 0.5 * Math.cos((seg.now % period) / period * 2 * Math.PI)) * (len - 1);
        // The trail: where the dot was a moment ago, fading.
        for (var i = 0; i < len; i++) {
            var best = 0;
            for (var k = 0; k < 12; k++) {
                var t = seg.now - k * period / 48;
                var p = (0.5 - 0.5 * Math.cos(((t % period) + period) % period / period * 2 * Math.PI)) * (len - 1);
                if (Math.abs(p - i) < 0.6)
                    best = Math.max(best, 1 - k / 12 * (len / 3) / tail);
            }
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(Math.round(pos)), Math.round(Math.max(0, best) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
