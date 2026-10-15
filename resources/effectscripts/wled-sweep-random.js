/*
  QLC+ Effect Script: WLED Sweep Random
  Sweep, in random colours: a colour fills the strip one way, the next fills it
  back the other way.
  Written from what WLED's "Sweep Random" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sweep Random";
    effect.description = "From WLED: a sweep back and forth in random colours";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sweep Random";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 400);
        var n = Math.floor(seg.now / period), ph = (seg.now % period) / period;
        var forward = (n % 2) === 0;
        var edge = (forward ? ph : 1 - ph) * len;
        var now = seg.randomColor(WLED.hash(n, 32)), was = seg.randomColor(WLED.hash(n - 1, 32));
        for (var i = 0; i < len; i++) {
            var reached = forward ? i < edge : i >= edge;
            seg.setPixelColor(i, reached ? now : was);
        }
        return seg.out();
    };

    return effect;
})();
