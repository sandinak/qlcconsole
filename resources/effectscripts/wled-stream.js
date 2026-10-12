/*
  QLC+ Effect Script: WLED Stream
  A stream of colour from the palette flowing along: patches of hue running past,
  each a few pixels long.
  Written from what WLED's "Stream" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Stream";
    effect.description = "From WLED: patches of colour streaming past";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Stream";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 400, 25);
        var step = Math.floor(seg.now / period);
        var size = 2 + Math.round(seg.intensity / 255 * 10);
        for (var i = 0; i < len; i++) {
            var p = i - step;
            var block = Math.floor(p / size);
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(block, 70) * 255), false, true, 0)
                                             : WLED.hue(Math.floor(WLED.hash(block, 70) * 256)));
        }
        return seg.out();
    };

    return effect;
})();
