/*
  QLC+ Effect Script: WLED Sunrise
  The strip brightens from black through deep red and orange to warm white, from the
  middle out, then sets again; speed is how long.
  Written from what WLED's "Sunrise" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sunrise";
    effect.description = "From WLED: dawn rising from the middle out, then setting";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sunrise";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 120000, 8000);
        var t = (seg.now % period) / period;
        var level = t < 0.5 ? t * 2 : 2 - t * 2;
        var ramp = [0x000000, 0x400000, 0xA01800, 0xFF6000, 0xFFB050, 0xFFF0D0];
        for (var i = 0; i < len; i++) {
            var fromMid = Math.abs(i - (len - 1) / 2) / Math.max(1, len / 2);
            var v = Math.max(0, Math.min(1, level * 1.4 - fromMid * 0.4)) * (ramp.length - 1);
            var k = Math.min(ramp.length - 2, Math.floor(v));
            seg.setPixelColor(i, WLED.colorBlend(ramp[k], ramp[k + 1], Math.round((v - k) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
