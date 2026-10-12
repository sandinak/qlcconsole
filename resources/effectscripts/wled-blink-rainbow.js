/*
  QLC+ Effect Script: WLED Blink Rainbow
  The whole strip blinks, a new colour of the wheel every blink.
  Written from what WLED's "Blink Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Blink Rainbow";
    effect.description = "From WLED: blinking through the rainbow";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Blink Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 2000, 100);
        var n = Math.floor(seg.now / period);
        var on = (seg.now % period) / period < 0.1 + seg.intensity / 255 * 0.8;
        seg.fill(on ? WLED.hue((n * 37) & 255) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
