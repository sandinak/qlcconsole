/*
  QLC+ Effect Script: WLED PS Chase
  Particles evenly spaced chasing along the strip, each a step further round the
  colour wheel (or palette) than the last; intensity is how close together.
  Written from what WLED's "PS Chase" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Chase";
    effect.description = "From WLED: evenly spaced particles chasing along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Chase";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var spacing = 3 + Math.floor((255 - seg.intensity) / 255 * 12);
        var v = len / (WLED.period(seg.speed, 8000, 600) / 1000);
        var offset = seg.now / 1000 * v;
        for (var i = 0; i < len; i++) {
            var p = (i - offset) / spacing;
            var k = Math.floor(p + 0.5);
            var lvl = Math.max(0, 1 - Math.abs(p - k) * spacing / 1.2);
            var h = k * 0.07;
            seg.setPixelColor(i, WLED.mix(seg.color(1), seg.randomColor(h - Math.floor(h)), lvl));
        }
        return seg.out();
    };

    return effect;
})();
