/*
  QLC+ Effect Script: WLED Two Areas
  Two blocks of colour 1 and colour 3 bouncing towards and away from each other.
  Written from what WLED's "Two Areas" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Two Areas";
    effect.description = "From WLED: two blocks moving in and out";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Two Areas";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 500);
        var s = 0.5 - 0.5 * Math.cos((seg.now % period) / period * 2 * Math.PI);
        var w = Math.max(1, Math.round(len * (0.08 + seg.intensity / 255 * 0.3)));
        var a = Math.round(s * (len / 2 - w)), b = len - w - a;
        seg.fill(seg.color(1));
        for (var k = 0; k < w; k++) {
            seg.setPixelColor(a + k, paint(a + k));
            seg.setPixelColor(b + k, seg.color(2) || 0x00FF00);
        }
        return seg.out();
    };

    return effect;
})();
