/*
  QLC+ Effect Script: WLED Spots
  Evenly spaced spots of colour 1 over colour 2, standing still; intensity is how many,
  speed how wide.
  Written from what WLED's "Spots" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Spots";
    effect.description = "From WLED: evenly spaced soft spots";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Spots";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = 1 + Math.round(seg.intensity / 255 * 9);
        var zone = len / count;
        var width = zone * (0.15 + 0.7 * (0.5 + 0.5 * Math.sin(seg.now / WLED.period(seg.speed, 8000, 800) * 2 * Math.PI)));
        for (var i = 0; i < len; i++) {
            var d = Math.abs((i % zone) - zone / 2) / (width / 2);
            seg.setPixelColor(i, d < 1 ? WLED.colorBlend(seg.color(1), paint(i), Math.round((1 - d * d) * 255)) : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
