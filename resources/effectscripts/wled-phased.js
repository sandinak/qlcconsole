/*
  QLC+ Effect Script: WLED Phased
  Two waves at slightly different sizes moving along together, beating against each
  other so bright and dark patches drift through.
  Written from what WLED's "Phased" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Phased";
    effect.description = "From WLED: two waves beating against each other";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Phased";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 600);
        var s = seg.now / period;
        var waves = 2 + seg.intensity / 255 * 6;
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var a = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * waves - s));
            var b = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * waves * 1.15 - s * 1.1));
            seg.setPixelColor(i, WLED.colorBlend(0, paint(i), Math.round(a * b * 255)));
        }
        return seg.out();
    };

    return effect;
})();
