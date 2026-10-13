/*
  QLC+ Effect Script: WLED Plasma
  Slowly shifting plasma: overlapping sine waves of colour mixing along the strip.
  Written from what WLED's "Plasma" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Plasma";
    effect.description = "From WLED: shifting plasma colours";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Plasma";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 30000, 2500);
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var v = Math.sin(2 * Math.PI * (x * 2 + s)) + Math.sin(2 * Math.PI * (x * 3.7 - s * 1.3))
                    + Math.sin(2 * Math.PI * (x * 1.3 + s * 0.7));
            var idx = Math.floor(((v + 3) / 6) * 255) & 0xFF;
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(idx, false, true, 0) : WLED.hue(idx));
        }
        return seg.out();
    };

    return effect;
})();
