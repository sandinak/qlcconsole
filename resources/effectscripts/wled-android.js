/*
  QLC+ Effect Script: WLED Android
  A segment of colour 1 runs round the strip, stretching and shrinking as it goes,
  over colour 2 -- like a phone's loading spinner.
  Written from what WLED's "Android" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Android";
    effect.description = "From WLED: a stretching, shrinking segment running round";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Android";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 500);
        var t = (seg.now % period) / period;
        var maxLen = 0.1 + seg.intensity / 255 * 0.6;
        var size = len * (0.05 + maxLen * (0.5 - 0.5 * Math.cos(t * 4 * Math.PI)));
        var start = (t * len * 1.5) % len;
        for (var i = 0; i < len; i++) {
            var d = ((i - start) % len + len) % len;
            seg.setPixelColor(i, d < size ? paint(i) : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
