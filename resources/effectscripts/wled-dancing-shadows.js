/*
  QLC+ Effect Script: WLED Dancing Shadows
  Spotlights of the palette's colours sweeping to and fro across each other, each at
  its own pace and width.
  Written from what WLED's "Dancing Shadows" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Dancing Shadows";
    effect.description = "From WLED: spotlights sweeping to and fro";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Dancing Shadows";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var spots = 2 + Math.round(seg.intensity / 255 * 6);
        var base = WLED.period(seg.speed, 10000, 1200);
        seg.fill(0);
        for (var k = 0; k < spots; k++) {
            var rate = base * (0.7 + 0.8 * WLED.hash(k, 50));
            var centre = (0.5 + 0.6 * Math.sin(seg.now / rate * 2 * Math.PI + k * 1.7)) * len;
            var width = len * (0.05 + 0.15 * WLED.hash(k, 51));
            var c = (seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(k, 52) * 255), false, true, 0) : WLED.hue(Math.floor(WLED.hash(k, 52) * 255)));
            for (var i = 0; i < len; i++) {
                var d = Math.abs(i - centre) / width;
                if (d < 1) {
                    var o = seg.getPixelColor(i), a = 1 - d;
                    seg.setPixelColor(i, WLED.rgb(Math.min(255, ((o >> 16) & 255) + ((c >> 16) & 255) * a),
                                                  Math.min(255, ((o >> 8) & 255) + ((c >> 8) & 255) * a),
                                                  Math.min(255, (o & 255) + (c & 255) * a)));
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
