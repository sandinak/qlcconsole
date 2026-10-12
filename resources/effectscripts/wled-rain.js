/*
  QLC+ Effect Script: WLED Rain
  Drops of colour 1 fall along the strip from the far end, over colour 2; intensity is
  how heavy the rain.
  Written from what WLED's "Rain" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Rain";
    effect.description = "From WLED: drops falling along the strip";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Rain";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var fall = WLED.period(seg.speed, 3000, 300);           // a drop's run
        var drops = 1 + Math.round(seg.intensity / 255 * 6);
        seg.fill(seg.color(1));
        for (var d = 0; d < drops; d++) {
            var off = WLED.hash(d, 1) * fall;
            var n = Math.floor((seg.now + off) / fall);
            var t = ((seg.now + off) % fall) / fall;
            var p = Math.round((len - 1) * (1 - t));
            var c = paint(p);
            seg.setPixelColor(p, c);
            seg.setPixelColor(p + 1, WLED.colorBlend(seg.color(1), c, 110));
            if (WLED.hash(n, d) < 0.5) seg.setPixelColor(p + 2, WLED.colorBlend(seg.color(1), c, 45));
        }
        return seg.out();
    };

    return effect;
})();
