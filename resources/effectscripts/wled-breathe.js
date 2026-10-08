/*
  QLC+ Effect Script: WLED Breathe
  WLED's "Breathe" (FX.cpp, mode_breath), ported: the colours breathe
  between the palette and colour 2, a quick swell and a long rest, at
  WLED's own speed. Run on a WLED lantern set to Breathe, it is how the rig
  view shows the lantern; run on any RGB fixtures, it is the same look.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Breathe";
    effect.description = "From WLED: swell to the colour and rest, at WLED speed";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Breathe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);

        var v = 0;
        var counter = (seg.now * ((seg.speed >> 3) + 10)) & 0xFFFF;
        counter = (counter >> 2) + (counter >> 4);      // 0-16384 + 0-2048
        if (counter < 16384) {
            if (counter > 8192) counter = 8192 - (counter - 8192);
            v = Math.trunc(WLED.sin16(counter) / 103);  // near parabolic, 0-224
        }
        var lum = (30 + v) & 0xFF;

        for (var i = 0; i < seg.len; i++)
            seg.setPixelColor(i, WLED.colorBlend(seg.colorFromPalette(i, true, seg.solidWrap, 0),
                                                 seg.color(1), 255 - lum));
        return seg.out();
    };

    return effect;
})();
