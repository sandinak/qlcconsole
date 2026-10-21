/*
  QLC+ Effect Script: WLED Fireworks
  Blobs of random colour light up here and there and fade away; intensity is how
  often, speed how quickly they fade.
  Written from what WLED's "Fireworks" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fireworks";
    effect.description = "From WLED: random colour blobs lighting and fading";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fireworks";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var slot = WLED.period(seg.intensity, 600, 40);         // a new blob every so often
        var fade = WLED.period(seg.speed, 2500, 300);
        seg.fill(seg.color(1));
        var cur = Math.floor(seg.now / slot);
        for (var s = cur - Math.ceil(fade / slot); s <= cur; s++) {
            var age = seg.now - s * slot;
            if (age < 0 || age >= fade) continue;
            var lvl = 1 - age / fade;
            var pos = Math.floor(WLED.hash(s, 36) * len);
            var c = seg.randomColor(WLED.hash(s, 37));
            for (var d = -1; d <= 1; d++)
                seg.setPixelColor(pos + d, WLED.add(seg.getPixelColor(pos + d), WLED.dim(c, d === 0 ? lvl : lvl * 0.4)));
        }
        return seg.out();
    };

    return effect;
})();
