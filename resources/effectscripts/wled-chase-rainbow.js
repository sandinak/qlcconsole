/*
  QLC+ Effect Script: WLED Chase Rainbow
  Like Chase, over a rainbow that turns as it goes.
  Written from what WLED's "Chase Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase Rainbow";
    effect.description = "From WLED: a chase over a turning rainbow";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 300);
        var t = (seg.now % period) / period;
        var size = Math.max(1, Math.round(1 + seg.intensity / 255 * len / 4));
        var head = Math.floor(t * len);
        for (var i = 0; i < len; i++) {
            var behind = ((head - i) % len + len) % len;
            seg.setPixelColor(i, behind < size ? seg.color(0) : WLED.hue(Math.floor(i / len * 256 + t * 256) & 255));
        }
        return seg.out();
    };

    return effect;
})();
