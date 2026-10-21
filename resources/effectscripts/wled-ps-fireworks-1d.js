/*
  QLC+ Effect Script: WLED PS Fireworks 1D
  A rocket shoots up from the start of the strip trailing exhaust, bursts at the
  top of its climb, and its sparks fly apart and fall back, fading; speed is how
  fast the rocket climbs, intensity how big the burst.
  Written from what WLED's "PS Fireworks 1D" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Fireworks 1D";
    effect.description = "From WLED: rockets bursting into falling sparks";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Fireworks 1D";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var cycle = 3500;
        var n = Math.floor(seg.now / cycle), t = (seg.now % cycle) / 1000;
        var apex = (0.5 + 0.4 * WLED.hash(n, 210)) * (len - 1);
        var tr = WLED.period(seg.speed, 1500, 500) / 1000;      // the climb
        var g = 2 * apex / (tr * tr);
        var c = seg.randomColor(WLED.hash(n, 211));
        seg.fill(seg.color(1));
        if (t < tr) {
            var y = g * tr * t - g * t * t / 2;
            for (var j = 3; j >= 0; j--) {
                var p = Math.round(y) - j;
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(j ? 0xFF8020 : 0xFFF0D0, 1 - j / 4)));
            }
        } else {
            var te = t - tr;
            var sparks = 8 + Math.floor(seg.intensity / 255 * 16);
            var vmax = len * (0.3 + seg.intensity / 255 * 0.5);
            var life = 1.8;
            if (te < life) {
                for (var k = 0; k < sparks; k++) {
                    var v = (WLED.hash(n, 212 + k) * 2 - 1) * vmax;
                    var q = Math.round(apex + v * te - g * 0.25 * te * te / 2);
                    seg.setPixelColor(q, WLED.add(seg.getPixelColor(q), WLED.dim(c, 1 - te / life)));
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
