/*
  QLC+ Effect Script: WLED Twinkleup
  Twinkles that fade in: pixels slowly brighten to their colour, then go out;
  intensity is how many.
  Written from what WLED's "Twinkleup" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Twinkleup";
    effect.description = "From WLED: twinkles fading in";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Twinkleup";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var base = WLED.period(seg.speed, 6000, 800);
        var density = 0.15 + 0.85 * seg.intensity / 255;
        for (var i = 0; i < len; i++) {
            var rate = base * (0.7 + 0.6 * WLED.hash(i, 100));
            var t = (seg.now + WLED.hash(i, 101) * rate) / rate;
            var n = Math.floor(t), ph = t - n;
            if (WLED.hash(i, n + 102) > density) { seg.setPixelColor(i, seg.color(1)); continue; }
            var lvl = ph < 0.75 ? Math.pow(Math.sin(ph / 0.75 * Math.PI / 2), 2) : 1 - (ph - 0.75) / 0.25;
            seg.setPixelColor(i, WLED.mix(seg.color(1), seg.randomColor(WLED.hash(i, n + 103)), lvl));
        }
        return seg.out();
    };

    return effect;
})();
