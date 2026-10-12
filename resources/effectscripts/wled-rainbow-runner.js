/*
  QLC+ Effect Script: WLED Rainbow Runner
  Rainbow pixels running along over colour 2, a new hue each step.
  Written from what WLED's "Rainbow Runner" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Rainbow Runner";
    effect.description = "From WLED: rainbow pixels running along";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Rainbow Runner";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 300);
        var head = Math.floor((seg.now % period) / period * len);
        var gap = 2 + Math.round((255 - seg.intensity) / 255 * 6);
        for (var i = 0; i < len; i++) {
            var p = ((i - head) % len + len) % len;
            seg.setPixelColor(i, (p % gap) === 0 ? WLED.hue(((i - p) * 23) & 255) : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
