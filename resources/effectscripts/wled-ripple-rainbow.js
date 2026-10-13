/*
  QLC+ Effect Script: WLED Ripple Rainbow
  Like Ripple, the ripples in rainbow colours over a background that slowly turns
  through the wheel.
  Written from what WLED's "Ripple Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Ripple Rainbow";
    effect.description = "From WLED: rainbow ripples over a turning background";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Ripple Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var life = WLED.period(seg.speed, 4000, 800);
        var count = 1 + Math.round(seg.intensity / 255 * 5);
        var bg = WLED.colorBlend(0, WLED.hue(Math.floor(seg.now / 120) & 255), 50);
        seg.fill(bg);
        for (var r = 0; r < count; r++) {
            var off = r / count * life;
            var n = Math.floor((seg.now + off) / life), t = ((seg.now + off) % life) / life;
            var centre = Math.floor(WLED.hash(n, r + 90) * len);
            var radius = t * len / 4;
            var c = WLED.hue(Math.floor(WLED.hash(n, r + 91) * 256));
            var a = Math.round((1 - t) * 255);
            seg.setPixelColor(Math.round(centre - radius), WLED.colorBlend(bg, c, a));
            seg.setPixelColor(Math.round(centre + radius), WLED.colorBlend(bg, c, a));
        }
        return seg.out();
    };

    return effect;
})();
