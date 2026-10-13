/*
  QLC+ Effect Script: WLED Solid Glitter
  Colour 1 still, with white glitter sparkling over it; intensity is how much.
  Written from what WLED's "Solid Glitter" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Solid Glitter";
    effect.description = "From WLED: still colour with glitter";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Solid Glitter";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 400, 30);
        var n = Math.floor(seg.now / period);
        seg.fill(seg.color(0));
        var count = Math.round(seg.intensity / 255 * len / 6) + 1;
        for (var k = 0; k < count; k++)
            if (WLED.hash(n, k + 7) < 0.6) seg.setPixelColor(Math.floor(WLED.hash(n, k) * len), 0xFFFFFF);
        return seg.out();
    };

    return effect;
})();
