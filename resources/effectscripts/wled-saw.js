/*
  QLC+ Effect Script: WLED Saw
  Ramps of colour 1 over colour 2 -- bright edge, fading back -- running along the strip;
  intensity is how many ramps there are.
  Written from what WLED's "Saw" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Saw";
    effect.description = "From WLED: sawtooth ramps of colour running along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Saw";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 6000, 300);
        var shift = (seg.now % period) / period;
        var teeth = 1 + Math.round(seg.intensity / 255 * 7);
        for (var i = 0; i < len; i++) {
            var x = i / len * teeth - shift * teeth;
            var ramp = x - Math.floor(x);                       // 0..1 up each tooth
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(ramp * 255)));
        }
        return seg.out();
    };

    return effect;
})();
