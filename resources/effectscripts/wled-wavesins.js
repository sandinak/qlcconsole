/*
  QLC+ Effect Script: WLED Wavesins
  The palette rippling: each pixel swings through the palette on a sine wave a little
  behind its neighbour.
  Written from what WLED's "Wavesins" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Wavesins";
    effect.description = "From WLED: palette swinging in sine waves";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Wavesins";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 500);
        var s = seg.now / period * 2 * Math.PI;
        var spread = 0.05 + seg.intensity / 255 * 0.5;
        for (var i = 0; i < len; i++) {
            var idx = Math.floor((0.5 + 0.5 * Math.sin(s - i * spread)) * 255);
            seg.setPixelColor(i, (seg.palette ? seg.colorFromPalette(idx, false, true, 0) : WLED.hue(idx)));
        }
        return seg.out();
    };

    return effect;
})();
