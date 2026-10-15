/*
  QLC+ Effect Script: WLED PS Pinball
  Balls shot up the strip from its start, falling back and bouncing until they
  come to rest, then shot again; speed is how hard they are shot, intensity how
  bouncy they are.
  Written from what WLED's "PS Pinball" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Pinball";
    effect.description = "From WLED: balls shot up and bouncing back";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Pinball";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var g = len * 1.5;
        var v0 = Math.sqrt(2 * g * len * (0.4 + 0.55 * seg.speed / 255));
        var keep = 0.5 + seg.intensity / 255 * 0.45;
        var per = 6000;
        seg.fill(seg.color(1));
        for (var k = 0; k < 3; k++) {
            var tk = seg.now + k * per / 3;
            var n = Math.floor(tk / per), t = (tk % per) / 1000;
            var v = v0 * (0.8 + 0.2 * WLED.hash(n * 3 + k, 240));
            var c = seg.randomColor(WLED.hash(n * 3 + k, 241));
            for (var j = 3; j >= 0; j--) {                      // a short trail
                var b = WLED.bounce(Math.max(0, t - j * 0.02), v, g, keep);
                var p = Math.round(Math.min(len - 1, b.pos));
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(c, 1 - j / 4)));
            }
        }
        return seg.out();
    };

    return effect;
})();
