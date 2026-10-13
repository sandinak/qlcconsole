/*
  QLC+ Effect Script: WLED Colorful
  Old-fashioned coloured bulbs -- red, amber, green, blue (or the palette) -- in a
  repeating row that steps along.
  Written from what WLED's "Colorful" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Colorful";
    effect.description = "From WLED: rows of coloured bulbs stepping along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Colorful";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var bulbs = [0xFF0000, 0xFFA000, 0x00C000, 0x0040FF];
        var period = WLED.period(seg.speed, 4000, 150);
        var step = Math.floor(seg.now / period);
        for (var i = 0; i < len; i++) {
            var k = ((i + step) % 4 + 4) % 4;
            seg.setPixelColor(i, seg.palette ? seg.colorFromPalette(k * 64, false, true, 0) : bulbs[k]);
        }
        return seg.out();
    };

    return effect;
})();
