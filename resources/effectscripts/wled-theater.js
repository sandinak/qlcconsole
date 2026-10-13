/*
  QLC+ Effect Script: WLED Theater
  Theatre marquee: one pixel lit, then a gap, repeated, the pattern stepping along;
  intensity sets the gap.
  Written from what WLED's "Theater" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Theater";
    effect.description = "From WLED: marquee lights stepping along";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Theater";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var gap = 2 + Math.round((255 - seg.intensity) / 255 * 6);    // 1 lit in 'gap'
        var period = WLED.period(seg.speed, 1200, 60);
        var offset = Math.floor(seg.now / period) % gap;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, ((i + gap - offset) % gap) === 0 ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
