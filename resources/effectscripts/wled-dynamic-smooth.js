/*
  QLC+ Effect Script: WLED Dynamic Smooth
  Like Dynamic, each pixel a random colour, but changing by cross-fading instead of
  jumping.
  Written from what WLED's "Dynamic Smooth" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Dynamic Smooth";
    effect.description = "From WLED: random colours cross-fading, pixel by pixel";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Dynamic Smooth";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 4000, 300);
        for (var i = 0; i < len; i++) {
            var t = (seg.now + WLED.hash(i, 140) * period) / period;
            var n = Math.floor(t), f = t - n;
            var a = WLED.hue(Math.floor(WLED.hash(i, n) * 256)), b = WLED.hue(Math.floor(WLED.hash(i, n + 1) * 256));
            seg.setPixelColor(i, WLED.colorBlend(a, b, Math.round(f * 255)));
        }
        return seg.out();
    };

    return effect;
})();
