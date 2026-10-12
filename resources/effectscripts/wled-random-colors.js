/*
  QLC+ Effect Script: WLED Random Colors
  The whole strip fades from one random colour to the next; speed sets how often,
  intensity how quickly each fade happens.
  Written from what WLED's "Random Colors" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Random Colors";
    effect.description = "From WLED: the strip drifts from one random colour to the next";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Random Colors";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 10000, 500);
        var step = Math.floor(seg.now / period);
        var t = (seg.now % period) / period;
        var fade = Math.min(1, t / Math.max(0.05, 1 - seg.intensity / 255 * 0.95));
        var from = WLED.hue(Math.floor(WLED.hash(step) * 256));
        var to = WLED.hue(Math.floor(WLED.hash(step + 1) * 256));
        seg.fill(WLED.colorBlend(from, to, Math.round(fade * 255)));
        return seg.out();
    };

    return effect;
})();
