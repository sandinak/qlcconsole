/*
  QLC+ Effect Script: WLED Strobe Rainbow
  Short flashes of the whole strip, each flash the next colour round the wheel.
  Written from what WLED's "Strobe Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Strobe Rainbow";
    effect.description = "From WLED: short flashes, each a new colour";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Strobe Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 2000, 50);
        var flash = Math.min(40, period / 2);
        var n = Math.floor(seg.now / period);
        var on = (seg.now % period) < flash;
        seg.fill(on ? WLED.hue((n * 37) & 0xFF) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
