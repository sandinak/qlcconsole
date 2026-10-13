/*
  QLC+ Effect Script: WLED Hyper Sparkle
  Like Sparkle, but several pixels flash white at once, faster; intensity is how many.
  Written from what WLED's "Hyper Sparkle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Hyper Sparkle";
    effect.description = "From WLED: many white flashes at once";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Hyper Sparkle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 400, 25);
        var n = Math.floor(seg.now / period);
        var count = 1 + Math.round(seg.intensity / 255 * len / 4);
        for (var i = 0; i < len; i++) seg.setPixelColor(i, paint(i));
        for (var k = 0; k < count; k++) seg.setPixelColor(Math.floor(WLED.hash(n, k) * len), 0xFFFFFF);
        return seg.out();
    };

    return effect;
})();
