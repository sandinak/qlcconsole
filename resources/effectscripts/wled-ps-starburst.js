/*
  QLC+ Effect Script: WLED PS Starburst
  Stars bursting at random places into fragments that fly apart, slow, sink a
  little and fade; speed is how often, intensity how many fragments.
  Written from what WLED's "PS Starburst" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Starburst";
    effect.description = "From WLED: stars bursting into fragments";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Starburst";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var slot = WLED.period(seg.speed, 2500, 250);
        var frags = 6 + Math.floor(seg.intensity / 255 * 14);
        var life = 1600;
        seg.fill(seg.color(1));
        var cur = Math.floor(seg.now / slot);
        for (var s = cur - Math.ceil(life / slot); s <= cur; s++) {
            if (WLED.hash(s, 260) > 0.7) continue;
            var age = (seg.now - s * slot) / life;
            if (age < 0 || age >= 1) continue;
            var ctr = (0.2 + 0.7 * WLED.hash(s, 261)) * (len - 1);
            var c = seg.randomColor(WLED.hash(s, 262));
            var out = (1 - Math.exp(-age * 3)) / (1 - Math.exp(-3));
            for (var f = 0; f < frags; f++) {
                var p = ctr + (WLED.hash(s, 270 + f) * 2 - 1) * len * 0.3 * out - age * age * len * 0.1;
                var lvl = (1 - age) * (1 - age);
                for (var d = 0; d <= 1; d++)
                    seg.setPixelColor(Math.round(p) + d, WLED.add(seg.getPixelColor(Math.round(p) + d), WLED.dim(c, d ? lvl * 0.5 : lvl)));
            }
        }
        return seg.out();
    };

    return effect;
})();
