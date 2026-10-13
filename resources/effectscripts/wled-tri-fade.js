/*
  QLC+ Effect Script: WLED Tri Fade
  The whole strip fades from colour 1 to colour 2 to colour 3 and round again.
  Written from what WLED's "Tri Fade" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Tri Fade";
    effect.description = "From WLED: fade through colours 1, 2 and 3";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Tri Fade";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 12000, 900);
        var t = (seg.now % period) / period * 3;
        var k = Math.floor(t), f = t - k;
        var c = [seg.color(0), seg.color(1), seg.color(2)];
        seg.fill(WLED.colorBlend(c[k % 3], c[(k + 1) % 3], Math.round(f * 255)));
        return seg.out();
    };

    return effect;
})();
