/*
  QLC+ Effect Script: WLED Scanner
  A bright dot sweeps end to end and back with a fading tail behind it, like a scanner
  bar; intensity is how long the tail is.
  Written from what WLED's "Scanner" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Scanner";
    effect.description = "From WLED: a dot sweeping back and forth with a tail";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Scanner";
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
        var pos = (forward ? ph : 2 - ph) * (len - 1);
        for (var i = 0; i < len; i++) {
            var behind = forward ? pos - i : i - pos;           // the tail trails the way it came
            var lvl = behind < -0.5 ? 0 : behind <= 0.5 ? 1 : Math.max(0, 1 - behind / tail);
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(lvl * 255)));
        }
        return seg.out();
    };

    return effect;
})();
