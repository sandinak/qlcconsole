/*
  QLC+ Effect Script: WLED Tri Wipe
  Colour 1 wipes along the strip, then colour 2, then colour 3, and round again.
  Written from what WLED's "Tri Wipe" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Tri Wipe";
    effect.description = "From WLED: wiping through colours 1, 2 and 3";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Tri Wipe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 400);
        var t = (seg.now % (3 * period)) / period;
        var k = Math.floor(t), f = t - k;
        var c = [seg.color(0), seg.color(1), seg.color(2)];
        var edge = Math.round(f * len);
        for (var i = 0; i < len; i++) seg.setPixelColor(i, i < edge ? c[(k + 1) % 3] : c[k]);
        return seg.out();
    };

    return effect;
})();
