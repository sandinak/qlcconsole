/*
  QLC+ Effect Script: WLED Ripple
  Ripples start at random places and spread outward, fading as they go; intensity is how
  many at a time.
  Written from what WLED's "Ripple" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Ripple";
    effect.description = "From WLED: ripples spreading from random places";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Ripple";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var life = WLED.period(seg.speed, 4000, 400);           // one ripple's life
        var count = 1 + Math.round(seg.intensity / 255 * 5);
        seg.fill(seg.color(1));
        for (var r = 0; r < count; r++) {
            var off = r * life / count;
            var n = Math.floor((seg.now + off) / life);
            var age = ((seg.now + off) % life) / life;          // 0..1
            var centre = WLED.hash(n, r) * (len - 1);
            var radius = age * len / 3;
            var col = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(n, r + 9) * 255), false, true, 0) : seg.color(0);
            for (var i = 0; i < len; i++) {
                var ring = 1 - Math.abs(Math.abs(i - centre) - radius) / 1.2;
                if (ring > 0) {
                    var a = Math.round(ring * (1 - age) * 255);
                    seg.setPixelColor(i, WLED.colorBlend(seg.getPixelColor(i), col, a));
                }
            }
        }
        return seg.out();
    };

    return effect;
})();
