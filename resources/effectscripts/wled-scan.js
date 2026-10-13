/*
  QLC+ Effect Script: WLED Scan
  A block of colour 1 sweeps end to end and back over colour 2; intensity is how wide
  the block is.
  Written from what WLED's "Scan" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Scan";
    effect.description = "From WLED: a block bouncing end to end";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Scan";
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
        var half = 0.5 + seg.intensity / 255 * len / 4;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, Math.abs(i - pos) <= half ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
