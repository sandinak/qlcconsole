/*
  QLC+ Effect Script: WLED Perlin Move
  Dots wandering up and down the strip on smooth noise, a short fading trail
  behind each; intensity is how many.
  Written from what WLED's "Perlin Move" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Perlin Move";
    effect.description = "From WLED: dots wandering on noise";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Perlin Move";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = 1 + Math.floor(seg.intensity / 255 * Math.max(1, len / 6));
        var s = seg.now / WLED.period(seg.speed, 20000, 1500);
        seg.fill(seg.color(1));
        for (var k = 0; k < count; k++) {
            for (var j = 6; j >= 0; j--) {
                var n = WLED.noise((s - j * 0.012) * 3, 170 + k);
                var p = Math.round(Math.max(0, Math.min(1, (n - 0.1) / 0.8)) * (len - 1));
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(paint(p), 1 - j / 7)));
            }
        }
        return seg.out();
    };

    return effect;
})();
