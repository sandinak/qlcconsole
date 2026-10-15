/*
  QLC+ Effect Script: WLED ICU
  Two eyes half a strip apart, glancing together to one spot and then another, now
  and then blinking; intensity is how often they blink.
  Written from what WLED's "ICU" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED ICU";
    effect.description = "From WLED: two eyes glancing about";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "ICU";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var hop = WLED.period(seg.speed, 4000, 400);            // a glance every so often
        var n = Math.floor(seg.now / hop), t = (seg.now % hop) / hop;
        var half = Math.floor(len / 2);
        var from = WLED.hash(n - 1, 38) * Math.max(0, half - 1), to = WLED.hash(n, 38) * Math.max(0, half - 1);
        var m = Math.min(1, t / 0.3);
        var pos = Math.round(from + (to - from) * m * m * (3 - 2 * m));
        var blink = WLED.hash(n, 39) < seg.intensity / 255 * 0.6 && t > 0.6 && t < 0.72;
        seg.fill(seg.color(1));
        if (!blink) {
            seg.setPixelColor(pos, paint(pos));
            seg.setPixelColor(pos + half, paint(pos + half));
        }
        return seg.out();
    };

    return effect;
})();
