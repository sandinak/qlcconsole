/*
  QLC+ Effect Script: WLED Sparkle Dark
  The strip in colour 1, with pixels blinking off to colour 2 at random.
  Written from what WLED's "Sparkle Dark" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sparkle Dark";
    effect.description = "From WLED: pixels blinking dark at random";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sparkle Dark";
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
        for (var k = 0; k < count; k++) seg.setPixelColor(Math.floor(WLED.hash(n, k + 99) * len), seg.color(1));
        return seg.out();
    };

    return effect;
})();
