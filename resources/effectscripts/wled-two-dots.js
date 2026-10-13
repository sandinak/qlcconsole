/*
  QLC+ Effect Script: WLED Two Dots
  Two dots -- colour 1 and colour 3 -- run the length of the strip in opposite
  directions over colour 2; intensity is how wide they are.
  Written from what WLED's "Two Dots" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Two Dots";
    effect.description = "From WLED: two dots running opposite ways";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Two Dots";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 6000, 400);
        var p = (seg.now % period) / period * len;
        var w = 0.5 + seg.intensity / 255 * len / 8;
        var other = seg.color(2) ? seg.color(2) : seg.color(0);
        for (var i = 0; i < len; i++) {
            var a = Math.abs(((i - p) % len + len + len / 2) % len - len / 2);
            var b = Math.abs(((i - (len - 1 - p)) % len + len + len / 2) % len - len / 2);
            seg.setPixelColor(i, a <= w ? paint(i) : b <= w ? other : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
