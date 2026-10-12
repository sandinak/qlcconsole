/*
  QLC+ Effect Script: WLED Multi Comet
  Several comets of colour 1 run along the strip at different times, each with a fading
  tail; intensity is the tail's length.
  Written from what WLED's "Multi Comet" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Multi Comet";
    effect.description = "From WLED: comets running along with tails";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Multi Comet";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var run = WLED.period(seg.speed, 5000, 400);
        var tail = 2 + seg.intensity / 255 * len / 3;
        seg.fill(seg.color(1));
        for (var k = 0; k < 6; k++) {
            var period = run * (1.2 + WLED.hash(k, 1));
            var t = ((seg.now + WLED.hash(k, 2) * period) % period) / period;
            var head = t * (len + tail);
            for (var i = 0; i < len; i++) {
                var behind = head - i;
                if (behind >= 0 && behind < tail) {
                    var a = Math.round((1 - behind / tail) * 255);
                    seg.setPixelColor(i, WLED.colorBlend(seg.getPixelColor(i), paint(i), a));
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
