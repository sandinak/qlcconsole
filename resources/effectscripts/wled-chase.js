/*
  QLC+ Effect Script: WLED Chase
  WLED's "Chase" (FX.cpp, mode_chase_color through chase()), ported: a
  block of colour 3 (or colour 1 when 3 is black) and a block of colour 1
  run round the palette, wrapping, the blocks as wide as the intensity says.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase";
    effect.description = "From WLED: two blocks of colour run round the palette";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var color2 = seg.color(2) ? seg.color(2) : seg.color(0);
        var color3 = seg.color(0);

        var counter = (seg.now * ((seg.speed >> 2) + 1)) & 0xFFFF;
        var a = (counter * len) >> 16;
        var size = 1 + ((seg.intensity * len) >> 10);
        var b = a + size;                    // the trail, in color2
        if (b > len) b -= len;
        var c = b + size;                    // then color3
        if (c > len) c -= len;

        for (var i = 0; i < len; i++)        // the background: the palette
            seg.setPixelColor(i, seg.colorFromPalette(i, true, seg.solidWrap, 0));

        var span = function(from, to, col) {
            if (from < to) { for (var i = from; i < to; i++) seg.setPixelColor(i, col); }
            else {
                for (var j = from; j < len; j++) seg.setPixelColor(j, col);
                for (var k = 0; k < to; k++) seg.setPixelColor(k, col);
            }
        };
        span(a, b, color2);
        span(b, c, color3);
        return seg.out();
    };

    return effect;
})();
