/*
  QLC+ Effect Script: WLED Chunchun
  Birds: a flock of dots in the palette's colours flying along at slightly different
  paces, bunching and spreading out.
  Written from what WLED's "Chunchun" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chunchun";
    effect.description = "From WLED: a flock of dots flying along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chunchun";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var birds = 2 + Math.round(seg.intensity / 255 * 12);
        var period = WLED.period(seg.speed, 12000, 1200);
        seg.fill(seg.color(1));
        for (var b = 0; b < birds; b++) {
            var lag = b / birds * 0.6;
            var t = seg.now / period - lag;
            var p = (0.5 + 0.5 * Math.sin(2 * Math.PI * t)) * (len - 1);
            seg.setPixelColor(Math.round(p), (seg.palette ? seg.colorFromPalette(Math.floor(b / birds * 255), false, true, 0) : WLED.hue(Math.floor(b / birds * 255))));
        }
        return seg.out();
    };

    return effect;
})();
