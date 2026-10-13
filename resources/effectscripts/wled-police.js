/*
  QLC+ Effect Script: WLED Police
  Red and blue lights chasing round the strip from opposite sides; intensity is their width.
  Written from what WLED's "Police" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Police";
    effect.description = "From WLED: red and blue chasing round";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Police";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 4000, 300);
        var p = (seg.now % period) / period * len;
        var w = 1 + Math.round(seg.intensity / 255 * len / 4);
        seg.fill(seg.color(1));
        for (var k = 0; k < w; k++) {
            var a = Math.floor(p + k) % len, b = (Math.floor(p + k) + Math.floor(len / 2)) % len;
            seg.setPixelColor(a, 0xFF0000);
            seg.setPixelColor(b, 0x0000FF);
        }
        return seg.out();
    };

    return effect;
})();
