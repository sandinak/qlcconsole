/*
  QLC+ Effect Script: WLED Scanner Dual
  Scanner with two dots, one from each end, sweeping past each other and back,
  each trailing a fading tail; intensity is the tail's length.
  Written from what WLED's "Scanner Dual" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Scanner Dual";
    effect.description = "From WLED: two scanning dots with fading tails";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Scanner Dual";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 400);
        var tail = 0.5 + seg.intensity / 255 * len / 2;
        var ph = (seg.now % (2 * period)) / period;
        var forward = ph < 1;
        var pos = (forward ? ph : 2 - ph) * (len - 1), pos2 = len - 1 - pos;
        var level = function(behind) {
            return behind < -0.5 ? 0 : behind <= 0.5 ? 1 : Math.max(0, 1 - behind / tail);
        };
        for (var i = 0; i < len; i++) {
            var a = level(forward ? pos - i : i - pos);
            var b = level(forward ? i - pos2 : pos2 - i);
            var other = seg.color(2) ? seg.color(2) : paint(i);
            seg.setPixelColor(i, a >= b ? WLED.mix(seg.color(1), paint(i), a) : WLED.mix(seg.color(1), other, b));
        }
        return seg.out();
    };

    return effect;
})();
