/*
  QLC+ Effect Script: WLED Dynamic
  Every pixel a random colour; every so often they all change at once to new random
  colours. Intensity is how smoothly they change (low: a cut, high: a fade).
  Written from what WLED's "Dynamic" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Dynamic";
    effect.description = "From WLED: every pixel a random colour, all changing together";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Dynamic";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 6000, 250);
        var step = Math.floor(seg.now / period);
        var t = (seg.now % period) / period;
        var fade = seg.intensity / 255;                         // the share of a step spent fading
        var a = fade > 0 ? Math.min(1, t / fade) : 1;
        for (var i = 0; i < len; i++) {
            var from = WLED.hue(Math.floor(WLED.hash(i, step - 1) * 256));
            var to = WLED.hue(Math.floor(WLED.hash(i, step) * 256));
            seg.setPixelColor(i, WLED.colorBlend(from, to, Math.round(a * 255)));
        }
        return seg.out();
    };

    return effect;
})();
