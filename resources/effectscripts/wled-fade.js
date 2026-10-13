/*
  QLC+ Effect Script: WLED Fade
  The whole strip fades smoothly back and forth between colour 1 and colour 2.
  Written from what WLED's "Fade" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fade";
    effect.description = "From WLED: fade between colours 1 and 2";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Fade";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 600);
        var ph = (seg.now % period) / period;
        var a = Math.round((0.5 - 0.5 * Math.cos(ph * 2 * Math.PI)) * 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), a));
        return seg.out();
    };

    return effect;
})();
