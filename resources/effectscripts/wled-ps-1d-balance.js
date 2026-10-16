/*
  QLC+ Effect Script: WLED PS 1D Balance
  Particles that slide back and forth along the strip as if it were being tilted,
  piling up at one end and then the other; speed is how fast it tilts, intensity
  how many particles.
  Written from what WLED's "PS 1D Balance" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS 1D Balance";
    effect.description = "From WLED: particles sliding as the strip tilts";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS 1D Balance";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = Math.max(1, Math.round(len * (0.1 + 0.5 * seg.intensity / 255)));
        var T = WLED.period(seg.speed, 12000, 1500);
        var u = 0.5 - 0.5 * Math.cos(seg.now / T * 2 * Math.PI);   // 0 tilted to the start, 1 to the end
        seg.fill(seg.color(1));
        for (var k = 0; k < count; k++) {
            var lead = count > 1 ? k / (count - 1) : 1;             // the end ones go first
            var p = Math.max(0, Math.min(1, (u - 0.3 * (1 - lead)) / 0.7));
            p = p * p * (3 - 2 * p);
            var x = Math.round(k + (len - count) * p);
            seg.setPixelColor(x, paint(Math.round(k * (len - 1) / Math.max(1, count))));
        }
        return seg.out();
    };

    return effect;
})();
