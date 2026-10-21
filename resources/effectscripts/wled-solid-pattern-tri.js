/*
  QLC+ Effect Script: WLED Solid Pattern Tri
  A still pattern of the three colours in turn, runs of the same size; intensity
  is the size.
  Written from what WLED's "Solid Pattern Tri" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Solid Pattern Tri";
    effect.description = "From WLED: a still pattern of three colours";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Solid Pattern Tri";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var size = 1 + Math.floor(seg.intensity / 16);
        for (var i = 0; i < len; i++) {
            var k = Math.floor(i / size) % 3;
            seg.setPixelColor(i, k === 0 ? paint(i) : seg.color(k));
        }
        return seg.out();
    };

    return effect;
})();
