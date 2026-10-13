/*
  QLC+ Effect Script: WLED Sweep
  Colour 1 fills the strip from the start, then empties back toward the start, over
  colour 2, back and forth.
  Written from what WLED's "Sweep" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sweep";
    effect.description = "From WLED: the colour fills the strip and sweeps back";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sweep";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 400);
        var ph = (seg.now % (2 * period)) / period;
        var edge = (ph < 1 ? ph : 2 - ph) * len;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, i < edge ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
