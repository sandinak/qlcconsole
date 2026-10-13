/*
  QLC+ Effect Script: WLED Solid
  The look's colour 1 (or its palette, laid across) on every pixel; nothing moves.
  Written from what WLED's "Solid" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Solid";
    effect.description = "From WLED: the colour, steady";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Solid";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, paint(i));
        return seg.out();
    };

    return effect;
})();
