/*
  QLC+ Effect Script: WLED PS Spray 1D
  A spray of particles thrown up from the start of the strip, falling back and
  bouncing, each fading as it ages; speed is how hard they are thrown, intensity
  how many.
  Written from what WLED's "PS Spray 1D" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Spray 1D";
    effect.description = "From WLED: a spray of particles thrown up";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Spray 1D";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var every = WLED.period(seg.intensity, 300, 20);        // one thrown every so often
        var v0 = len * (0.5 + 1.5 * seg.speed / 255);
        var g = len * 1.2;
        var life = 2500;
        seg.fill(seg.color(1));
        var cur = Math.floor(seg.now / every);
        for (var s = cur - Math.ceil(life / every); s <= cur; s++) {
            var age = seg.now - s * every;
            if (age < 0 || age >= life) continue;
            var b = WLED.bounce(age / 1000, v0 * (0.6 + 0.4 * WLED.hash(s, 220)), g, 0.5);
            var p = Math.round(Math.min(len - 1, b.pos));
            seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(seg.randomColor(WLED.hash(s, 221)), 1 - age / life)));
        }
        return seg.out();
    };

    return effect;
})();
