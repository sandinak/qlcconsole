/*
  QLC+ Effect Script: WLED Twinkle
  Pixels light one by one at random, in colour 1, until the strip has filled; then it
  clears and starts again. Intensity is how full it gets.
  Written from what WLED's "Twinkle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Twinkle";
    effect.description = "From WLED: random pixels light up until it fills, then start over";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Twinkle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 900, 40);           // one more pixel each
        var fullAt = Math.max(1, Math.round(len * (0.1 + seg.intensity / 255 * 0.9)));
        var n = Math.floor(seg.now / period);
        var round = Math.floor(n / fullAt), lit = n % fullAt + 1;
        seg.fill(seg.color(1));
        for (var k = 0; k < lit; k++) {
            var i = Math.floor(WLED.hash(round, k) * len);
            seg.setPixelColor(i, paint(i));
        }
        return seg.out();
    };

    return effect;
})();
