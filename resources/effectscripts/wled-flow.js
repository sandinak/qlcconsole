/*
  QLC+ Effect Script: WLED Flow
  The palette flowing along the strip, with every other zone running the other way.
  Written from what WLED's "Flow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Flow";
    effect.description = "From WLED: palette flowing, alternate zones reversed";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Flow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 10000, 800);
        var s = (seg.now % period) / period * 256;
        var zones = 2 + Math.round(seg.intensity / 255 * 8);
        var zone = len / zones;
        for (var i = 0; i < len; i++) {
            var z = Math.floor(i / zone);
            var idx = (z % 2) ? (s - i / len * 256) : (s + i / len * 256);
            seg.setPixelColor(i, (seg.palette ? seg.colorFromPalette(Math.floor((idx % 256 + 256) % 256), false, true, 0) : WLED.hue(Math.floor((idx % 256 + 256) % 256))));
        }
        return seg.out();
    };

    return effect;
})();
