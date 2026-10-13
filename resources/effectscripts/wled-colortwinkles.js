/*
  QLC+ Effect Script: WLED Colortwinkles
  Pixels slowly brighten into colours of the palette (or wheel) and fade away again,
  each in its own time; intensity is how many.
  Written from what WLED's "Colortwinkles" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Colortwinkles";
    effect.description = "From WLED: pixels slowly glowing up in colour and fading";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Colortwinkles";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var base = WLED.period(seg.speed, 8000, 1000);
        var density = 0.2 + seg.intensity / 255 * 0.8;
        for (var i = 0; i < len; i++) {
            var rate = base * (0.7 + 0.6 * WLED.hash(i, 80));
            var t = (seg.now + WLED.hash(i, 81) * rate) / rate;
            var n = Math.floor(t), ph = t - n;
            if (WLED.hash(i, n + 82) > density) { seg.setPixelColor(i, seg.color(1)); continue; }
            var lvl = ph < 0.4 ? ph / 0.4 : Math.max(0, 1 - (ph - 0.4) / 0.6);
            var c = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(i, n + 83) * 255), false, true, 0) : WLED.hue(Math.floor(WLED.hash(i, n + 83) * 256));
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), c, Math.round(lvl * 255)));
        }
        return seg.out();
    };

    return effect;
})();
