/*
  QLC+ Effect Script: WLED Running
  Smooth waves of colour 1 and colour 2 run along the strip; intensity is how wide the
  waves are.
  Written from what WLED's "Running" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Running";
    effect.description = "From WLED: soft waves of colour running along";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Running";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 400);
        var shift = (seg.now % period) / period;
        var waves = 0.5 + (255 - seg.intensity) / 255 * 7.5;
        for (var i = 0; i < len; i++) {
            var s = 0.5 + 0.5 * Math.sin(2 * Math.PI * (i / len * waves - shift));
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(s * 255)));
        }
        return seg.out();
    };

    return effect;
})();
