/*
  QLC+ Effect Script: WLED PS Fire 1D
  Fire rising from the start of the strip: hot sparks thrown up from the base,
  cooling from white through yellow and red to nothing as they rise; speed is how
  fast the flames rise, intensity how hot.
  Written from what WLED's "PS Fire 1D" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Fire 1D";
    effect.description = "From WLED: particle fire rising from the start";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Fire 1D";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var heat = [];
        for (var i = 0; i < len; i++) heat.push(0);
        var step = 25, cur = Math.floor(seg.now / step);
        var vmax = len * (0.4 + 0.8 * seg.speed / 255);
        var hot = 0.45 + 0.55 * seg.intensity / 255;
        for (var s = cur - 48; s <= cur; s++) {
            for (var j = 0; j < 3; j++) {
                var id = s * 3 + j;
                var life = 0.6 + 0.6 * WLED.hash(id, 280);
                var age = (seg.now - s * step) / 1000;
                if (age < 0 || age >= life) continue;
                var v = vmax * (0.4 + 0.6 * WLED.hash(id, 281));
                var p = v * age * (1 - age / (2 * life));               // slowing as it cools
                var h = (1 - age / life) * hot;
                var k = Math.floor(p), w = p - k;
                if (k >= 0 && k < len) heat[k] += h * (1 - w);
                if (k + 1 < len) heat[k + 1] += h * w;
            }
        }
        for (var i = 0; i < len; i++) {
            var x = Math.min(1, heat[i] * 0.6);
            var c = seg.palette ? WLED.dim(seg.colorFromPalette(Math.floor(x * 240), false, false, 0), Math.min(1, x * 3))
                                : WLED.heatColor(x);
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
