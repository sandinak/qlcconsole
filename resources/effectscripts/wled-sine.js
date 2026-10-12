/*
  QLC+ Effect Script: WLED Sine
  A sine wave of colour 1 travelling along the strip; intensity is how many waves fit.
  Written from what WLED's "Sine" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sine";
    effect.description = "From WLED: a sine wave of colour travelling along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sine";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 500);
        var shift = (seg.now % period) / period;
        var waves = 1 + seg.intensity / 255 * 7;
        for (var i = 0; i < len; i++) {
            var s = 0.5 + 0.5 * Math.sin(2 * Math.PI * (i / len * waves - shift));
            seg.setPixelColor(i, WLED.colorBlend(0, paint(i), Math.round(s * 255)));
        }
        return seg.out();
    };

    return effect;
})();
