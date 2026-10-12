/*
  QLC+ Effect Script: WLED Glitter
  A rainbow rolling along the strip with white glitter sparkling over it; intensity is
  how much glitter.
  Written from what WLED's "Glitter" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Glitter";
    effect.description = "From WLED: a rainbow with glitter";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Glitter";
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
        var flick = Math.floor(seg.now / 50);
        for (var i = 0; i < len; i++) {
            var c = seg.palette ? seg.colorFromPalette(Math.floor(shift + i / len * 256), false, true, 0)
                                : WLED.hue(Math.floor(shift + i / len * 256));
            if (WLED.hash(i, flick) < seg.intensity / 255 * 0.15)
                c = 0xFFFFFF;
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
