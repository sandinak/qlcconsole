/*
  QLC+ Effect Script: WLED Palette
  The palette laid along the strip, scrolling; intensity is how much of the palette fits
  on the strip at once.
  Written from what WLED's "Palette" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Palette";
    effect.description = "From WLED: the palette scrolling along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Palette";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 20000, 1000);
        var shift = (seg.now % period) / period * 256;
        var span = 0.25 + seg.intensity / 255 * 1.75;
        for (var i = 0; i < len; i++) {
            var idx = Math.floor(shift + i / len * 256 * span) & 0xFF;
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(idx, false, true, 0)
                                             : WLED.colorBlend(seg.color(0), seg.color(2) || seg.color(1),
                                                               Math.round((0.5 - 0.5 * Math.cos(idx / 256 * 2 * Math.PI)) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
