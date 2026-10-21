/*
  QLC+ Effect Script: WLED Fireworks Starburst
  Stars bursting at random places: fragments fly out both ways, slowing as they
  go, white-hot at first and fading out in their colour; speed is how often,
  intensity how many fragments.
  Written from what WLED's "Fireworks Starburst" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fireworks Starburst";
    effect.description = "From WLED: exploding multicolour stars";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fireworks Starburst";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var slot = WLED.period(seg.speed, 2500, 250);           // a chance of a star
        var frags = 4 + Math.floor(seg.intensity / 255 * 12);
        var life = 2000;
        seg.fill(seg.color(1));
        var cur = Math.floor(seg.now / slot);
        for (var s = cur - Math.ceil(life / slot); s <= cur; s++) {
            if (WLED.hash(s, 50) > 0.75) continue;              // not every chance fires
            var age = (seg.now - s * slot) / life;
            if (age < 0 || age >= 1) continue;
            var ctr = (0.15 + 0.7 * WLED.hash(s, 51)) * (len - 1);
            var c = seg.randomColor(WLED.hash(s, 52));
            if (age < 0.15) c = WLED.colorBlend(c, 0xFFFFFF, Math.round((1 - age / 0.15) * 200));
            var lvl = (1 - age) * (1 - age);
            var out = (1 - Math.exp(-age * 4)) / (1 - Math.exp(-4));
            for (var f = 0; f < frags; f++) {
                var p = ctr + (WLED.hash(s, 60 + f) * 2 - 1) * len * 0.35 * out;
                var k = Math.floor(p), w = p - k;
                seg.setPixelColor(k, WLED.add(seg.getPixelColor(k), WLED.dim(c, lvl * (1 - w))));
                seg.setPixelColor(k + 1, WLED.add(seg.getPixelColor(k + 1), WLED.dim(c, lvl * w)));
            }
        }
        return seg.out();
    };

    return effect;
})();
