/*
  QLC+ Effect Script: WLED Aurora
  Northern lights: soft curtains of green, teal and violet (or the palette) drifting
  along the strip, brightening and fading as they go.
  Written from what WLED's "Aurora" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Aurora";
    effect.description = "From WLED: drifting curtains of aurora colour";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Aurora";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 40000, 4000);
        var waves = 2 + Math.round(seg.intensity / 255 * 6);
        var cols = [0x00FF60, 0x00C0A0, 0x6020FF, 0x30FF20];
        for (var i = 0; i < len; i++) seg.setPixelColor(i, seg.color(1));
        for (var w = 0; w < waves; w++) {
            var centre = (WLED.noise(s * 0.6 + w * 3.1, 30 + w) * 1.4 - 0.2) * len;
            var width = len * (0.08 + 0.15 * WLED.hash(w, 31));
            var bright = WLED.noise(s * 1.3 + w, 40 + w);
            var c = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(w, 32) * 255), false, true, 0) : cols[w % 4];
            for (var i = 0; i < len; i++) {
                var d = Math.abs(i - centre) / width;
                if (d < 1) {
                    var a = (1 - d * d) * bright;
                    seg.setPixelColor(i, WLED.colorBlend(seg.getPixelColor(i), c, Math.round(a * 255)));
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
