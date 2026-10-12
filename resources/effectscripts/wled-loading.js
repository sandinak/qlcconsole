/*
  QLC+ Effect Script: WLED Loading
  Like Gradient, but the band's back edge is sharp -- a progress bar sweeping along.
  Written from what WLED's "Loading" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Loading";
    effect.description = "From WLED: a sweeping progress bar";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Loading";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 600);
        var head = (seg.now % period) / period * len;
        var soft = 2 + seg.intensity / 255 * len / 2;
        for (var i = 0; i < len; i++) {
            var behind = ((head - i) % len + len) % len;
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(Math.max(0, 1 - behind / soft) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
