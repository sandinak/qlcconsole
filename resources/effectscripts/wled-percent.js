/*
  QLC+ Effect Script: WLED Percent
  Lights a share of the strip from its start: intensity is the share (0 none, 255
  all), the lit part in the palette (or colour 1), the rest colour 2.
  Written from what WLED's "Percent" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Percent";
    effect.description = "From WLED: a share of the strip lit";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Percent";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var lit = Math.round(seg.intensity / 255 * len);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, i < lit ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
