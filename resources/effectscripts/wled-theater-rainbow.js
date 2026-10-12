/*
  QLC+ Effect Script: WLED Theater Rainbow
  The theatre marquee, its lit pixels in rainbow colours that roll along the strip.
  Written from what WLED's "Theater Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Theater Rainbow";
    effect.description = "From WLED: rainbow marquee lights stepping along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Theater Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var gap = 2 + Math.round((255 - seg.intensity) / 255 * 6);
        var period = WLED.period(seg.speed, 1200, 60);
        var offset = Math.floor(seg.now / period) % gap;
        var shift = seg.now / (period * 20) * 256;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, ((i + gap - offset) % gap) === 0
                                 ? WLED.hue(Math.floor(shift + i / len * 256)) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
