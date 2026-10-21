/*
  QLC+ Effect Script: WLED PS Sparkler
  Sparklers moving up and down the strip, throwing off sparks that fly out and
  fade; speed is how fast the sparklers move, intensity how quickly the sparks
  fade.
  Written from what WLED's "PS Sparkler" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Sparkler";
    effect.description = "From WLED: moving sparklers throwing sparks";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Sparkler";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var life = WLED.period(seg.intensity, 1200, 200);
        var per = WLED.period(seg.speed, 12000, 1500);
        var emitter = function(k, ms) {
            var u = ms / per + WLED.hash(k, 250);
            var f = u - 2 * Math.floor(u / 2);
            return (f < 1 ? f : 2 - f) * (len - 1);
        };
        seg.fill(seg.color(1));
        var step = 15, cur = Math.floor(seg.now / step);
        for (var k = 0; k < 2; k++) {
            for (var s = cur - Math.ceil(life / step); s <= cur; s++) {
                var age = seg.now - s * step;
                if (age < 0 || age >= life) continue;
                for (var j = 0; j < 2; j++) {
                    var v = (WLED.hash(s * 4 + k * 2 + j, 251) * 2 - 1) * len * 0.6;
                    var p = Math.round(emitter(k, s * step) + v * age / 1000);
                    var c = WLED.colorBlend(0xFFFFFF, seg.randomColor(WLED.hash(s + k, 252)), 128);
                    seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(c, 1 - age / life)));
                }
            }
            var e = Math.round(emitter(k, seg.now));
            seg.setPixelColor(e, 0xFFFFFF);
        }
        return seg.out();
    };

    return effect;
})();
