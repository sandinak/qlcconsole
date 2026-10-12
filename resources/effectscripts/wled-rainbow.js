/*
  QLC+ Effect Script: WLED Rainbow
  A rainbow laid along the strip, rolling along it; intensity is how many rainbows fit
  on the strip.
  Written from what WLED's "Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Rainbow";
    effect.description = "From WLED: a rainbow rolling along the strip";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Rainbow";
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
        var across = 0.25 + seg.intensity / 255 * 3.75;      // rainbows on the strip
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, WLED.hue(Math.floor(shift + i / len * 256 * across)));
        return seg.out();
    };

    return effect;
})();
