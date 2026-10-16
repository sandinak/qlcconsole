/*
  QLC+ Effect Script: WLED Chase 3
  Chase with three colours: blocks of the palette (or colour 1), colour 2 and
  colour 3 in turn, moving along the strip; intensity is their size.
  Written from what WLED's "Chase 3" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase 3";
    effect.description = "From WLED: blocks of three colours moving along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase 3";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 400, 25);
        var step = Math.floor(seg.now / period);
        var size = 1 + Math.floor(seg.intensity / 255 * Math.max(1, len / 4));
        for (var i = 0; i < len; i++) {
            var k = ((Math.floor((i - step) / size) % 3) + 3) % 3;
            seg.setPixelColor(i, k === 0 ? paint(i) : seg.color(k));
        }
        return seg.out();
    };

    return effect;
})();
