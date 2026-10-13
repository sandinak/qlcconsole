/*
  QLC+ Effect Script: WLED Running Dual
  Two sets of soft waves -- colour 1 and colour 3 -- running in opposite directions
  through each other.
  Written from what WLED's "Running Dual" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Running Dual";
    effect.description = "From WLED: waves of two colours running opposite ways";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Running Dual";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 400);
        var s = (seg.now % period) / period;
        var waves = 0.5 + (255 - seg.intensity) / 255 * 7.5;
        var other = seg.color(2) || seg.color(0);
        for (var i = 0; i < len; i++) {
            var x = i / len * waves;
            var a = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x - s));
            var b = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x + s));
            var c = WLED.colorBlend(seg.color(1), paint(i), Math.round(a * 255));
            seg.setPixelColor(i, WLED.colorBlend(c, other, Math.round(b * 0.5 * 255)));
        }
        return seg.out();
    };

    return effect;
})();
