/*
  QLC+ Effect Script: WLED Sparkle+
  The strip lit in the palette (or colour 1), and every moment several random
  pixels drop out for an instant; intensity is how many.
  Written from what WLED's "Sparkle+" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sparkle+";
    effect.description = "From WLED: lit, with many pixels flicking out at random";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sparkle+";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 300, 20);
        var n = Math.floor(seg.now / period);
        var count = 1 + Math.round(seg.intensity / 255 * len / 3);
        for (var i = 0; i < len; i++) seg.setPixelColor(i, paint(i));
        for (var k = 0; k < count; k++) seg.setPixelColor(Math.floor(WLED.hash(n, k + 40) * len), seg.color(1));
        return seg.out();
    };

    return effect;
})();
