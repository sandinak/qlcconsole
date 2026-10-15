/*
  QLC+ Effect Script: WLED Wipe Random
  Wipe, in a new random colour each time: each wipe runs a fresh colour along the
  strip over the last one.
  Written from what WLED's "Wipe Random" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Wipe Random";
    effect.description = "From WLED: a wipe in a new random colour each time";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Wipe Random";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 400);
        var n = Math.floor(seg.now / period);
        var edge = (seg.now % period) / period * len;
        var now = seg.randomColor(WLED.hash(n, 31)), was = seg.randomColor(WLED.hash(n - 1, 31));
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, i < edge ? now : was);
        return seg.out();
    };

    return effect;
})();
