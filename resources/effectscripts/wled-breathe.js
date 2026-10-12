/*
  QLC+ Effect Script: WLED Breathe
  The whole strip breathes: a soft swell up to colour 1 (or the palette) and back down
  toward colour 2, at the speed set.
  Written from what WLED's "Breathe" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Breathe";
    effect.description = "From WLED: swell to the colour and back, like breathing";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Breathe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 9000, 900);
        var ph = (seg.now % period) / period;
        // A breath: most of the time near the floor, a rounded swell.
        var lvl = Math.pow(0.5 - 0.5 * Math.cos(ph * 2 * Math.PI), 1.6);
        var a = Math.round(lvl * 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), a));
        return seg.out();
    };

    return effect;
})();
