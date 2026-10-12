/*
  QLC+ Effect Script: WLED Wipe
  Colour 1 wipes in from the start of the strip to the end, then colour 2 wipes in after
  it, over and over.
  Written from what WLED's "Wipe" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Wipe";
    effect.description = "From WLED: colour 1 then colour 2 wipe along the strip";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Wipe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 400);
        var ph = (seg.now % (2 * period)) / period;          // 0..2: two wipes
        var second = ph >= 1;
        var edge = (second ? ph - 1 : ph) * len;
        for (var i = 0; i < len; i++) {
            var ahead = i < edge;
            var c = second ? (ahead ? seg.color(1) : paint(i)) : (ahead ? paint(i) : seg.color(1));
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
