/*
  QLC+ Effect Script: WLED Scan Dual
  Scan with two lights, one from each end, crossing in the middle and bouncing
  back; intensity is their size.
  Written from what WLED's "Scan Dual" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Scan Dual";
    effect.description = "From WLED: two lights scanning from both ends";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Scan Dual";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 400);
        var ph = (seg.now % (2 * period)) / period;
        var pos = (ph < 1 ? ph : 2 - ph) * (len - 1);
        var half = 0.5 + seg.intensity / 255 * len / 8;
        for (var i = 0; i < len; i++) {
            var c = seg.color(1);
            if (Math.abs(i - (len - 1 - pos)) <= half) c = seg.color(2) ? seg.color(2) : paint(i);
            if (Math.abs(i - pos) <= half) c = paint(i);
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
