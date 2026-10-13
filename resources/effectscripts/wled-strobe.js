/*
  QLC+ Effect Script: WLED Strobe
  The whole strip flashes colour 1 briefly, colour 2 between flashes; speed is the
  flash rate.
  Written from what WLED's "Strobe" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Strobe";
    effect.description = "From WLED: short flashes of the colour";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Strobe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 2000, 50);
        var flash = Math.min(40, period / 2);                   // a flash is short
        var on = (seg.now % period) < flash;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, on ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
