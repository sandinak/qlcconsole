/*
  QLC+ Effect Script: WLED Chase
  A block of colour 3 (colour 1 when 3 is black) runs round over the palette --
  colour 1 when none is set -- with a block of colour 1 behind it; intensity
  is how long the blocks are.
  Written from what WLED's "Chase" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase";
    effect.description = "From WLED: a block of colour running round the palette";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Chase";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 8000, 300);
        var head = (seg.now % period) / period * len;
        var size = Math.max(1, Math.round(len * (0.05 + seg.intensity / 255 * 0.3)));
        var lead = seg.color(2) ? seg.color(2) : seg.color(0);
        for (var i = 0; i < len; i++) {
            var back = ((head - i) % len + len) % len;          // how far behind the head
            var c = paint(i);                                   // the palette (or colour 1)
            if (back < size) c = lead;
            else if (back < 2 * size) c = seg.color(0);
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
