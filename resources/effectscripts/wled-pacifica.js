/*
  QLC+ Effect Script: WLED Pacifica
  Gentle ocean: layers of blue-green waves sliding over each other at different
  speeds, with whitecaps where they meet.
  Written from what WLED's "Pacifica" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Pacifica";
    effect.description = "From WLED: slow layered ocean waves";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Pacifica";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 30000, 4000);
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var w1 = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * 1.3 + s * 0.9));
            var w2 = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * 2.7 - s * 1.4 + 0.3));
            var w3 = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * 4.1 + s * 2.2 + 0.7));
            var deep = WLED.rgb(0, 20 + Math.round(w1 * 40), 60 + Math.round(w2 * 80));
            var c = WLED.colorBlend(deep, 0x00A0B0, Math.round(w2 * w3 * 160));
            var cap = Math.max(0, w1 + w2 + w3 - 2.4) / 0.6;      // where the waves pile up
            seg.setPixelColor(i, WLED.colorBlend(c, 0xC0FFFF, Math.round(cap * 200)));
        }
        return seg.out();
    };

    return effect;
})();
