/*
  QLC+ Effect Script: WLED Blink
  Every pixel blinks between colour 1 and colour 2; speed sets the rate, intensity how
  long each blink stays on.
  Written from what WLED's "Blink" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Blink";
    effect.description = "From WLED: on and off between colours 1 and 2";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Blink";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 2000, 120);
        var on = ((seg.now % period) / period) < (0.05 + 0.9 * seg.intensity / 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, on ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
