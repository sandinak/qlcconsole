/*
  QLC+ Effect Script: WLED Gradient
  A bright band of colour 1 moving along the strip, fading softly into colour 2 at
  its edges; intensity is how wide the fade.
  Written from what WLED's "Gradient" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Gradient";
    effect.description = "From WLED: a band of colour with soft edges, moving";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Gradient";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 600);
        var centre = (seg.now % period) / period * len;
        var soft = 2 + seg.intensity / 255 * len / 2;
        for (var i = 0; i < len; i++) {
            var d = Math.abs(i - centre); d = Math.min(d, len - d);
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(Math.max(0, 1 - d / soft) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
