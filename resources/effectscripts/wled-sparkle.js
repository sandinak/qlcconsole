/*
  QLC+ Effect Script: WLED Sparkle
  Colour 2 everywhere, and a single random pixel flashing colour 1 at a time; speed is
  how fast the sparkle moves on.
  Written from what WLED's "Sparkle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sparkle";
    effect.description = "From WLED: one random pixel flashing over the background";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Sparkle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 800, 20);
        var step = Math.floor(seg.now / period);
        seg.fill(seg.color(1));
        var i = Math.floor(WLED.hash(step) * len);
        seg.setPixelColor(i, paint(i));
        return seg.out();
    };

    return effect;
})();
