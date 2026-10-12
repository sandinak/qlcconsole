/*
  QLC+ Effect Script: WLED Chase Flash
  Colour 1 over the strip, with a bright double flash of white that hops along.
  Written from what WLED's "Chase Flash" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase Flash";
    effect.description = "From WLED: a double white flash hopping along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase Flash";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 1500, 120);
        var n = Math.floor(seg.now / period), t = (seg.now % period) / period;
        for (var i = 0; i < len; i++) seg.setPixelColor(i, paint(i));
        var flash = (t < 0.15) || (t > 0.3 && t < 0.45);
        if (flash) {
            var at = (n * 3) % len;
            for (var k = 0; k < 3; k++) seg.setPixelColor((at + k) % len, 0xFFFFFF);
        }
        return seg.out();
    };

    return effect;
})();
