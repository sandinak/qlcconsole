/*
  QLC+ Effect Script: WLED Solid Pattern
  A still pattern: a run of pixels in the palette (or colour 1), then a run of
  colour 2, over and over; speed is how many lit, intensity how many in colour 2.
  Written from what WLED's "Solid Pattern" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Solid Pattern";
    effect.description = "From WLED: a still pattern of lit and unlit runs";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Solid Pattern";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var on = 1 + Math.floor(seg.speed / 8);
        var off = Math.floor(seg.intensity / 8);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, (i % (on + off)) < on ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
