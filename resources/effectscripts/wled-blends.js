/*
  QLC+ Effect Script: WLED Blends
  Random colours off the palette blending into each other across the strip, slowly
  shifting along; speed is the shift, intensity how quickly the colours change.
  Written from what WLED's "Blends" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Blends";
    effect.description = "From WLED: random palette colours blending across the strip";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Blends";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var shift = seg.now / WLED.period(seg.speed, 20000, 1000);
        var tb = WLED.period(seg.intensity, 6000, 400);
        var m = Math.floor(seg.now / tb), fm = (seg.now % tb) / tb;
        var size = Math.max(2, len / 6);
        var at = function(k) {
            return WLED.colorBlend(seg.randomColor(WLED.hash(k * 97 + m, 160)),
                                   seg.randomColor(WLED.hash(k * 97 + m + 1, 160)), Math.round(fm * 255));
        };
        for (var i = 0; i < len; i++) {
            var p = i / size + shift;
            var k = Math.floor(p), f = p - k;
            seg.setPixelColor(i, WLED.colorBlend(at(k), at(k + 1), Math.round(f * f * (3 - 2 * f) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
