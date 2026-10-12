/*
  QLC+ Effect Script: WLED Fairy
  Fairy lights: a warm glow of colour 1 with random bulbs brightening and settling back;
  intensity is how many at a time.
  Written from what WLED's "Fairy" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fairy";
    effect.description = "From WLED: fairy lights glowing and glinting";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fairy";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 3000, 300);
        var dim = WLED.colorBlend(0, seg.color(0) || 0xFFB040, 90);
        for (var i = 0; i < len; i++) {
            var n = Math.floor((seg.now + WLED.hash(i, 8) * period) / period);
            var ph = ((seg.now + WLED.hash(i, 8) * period) % period) / period;
            var glint = WLED.hash(i, n) < seg.intensity / 255 * 0.5 ? Math.sin(ph * Math.PI) : 0;
            var c = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(i, 9) * 255), false, true, 0) : (seg.color(0) || 0xFFB040);
            seg.setPixelColor(i, WLED.colorBlend(dim, c, Math.round(glint * 255)));
        }
        return seg.out();
    };

    return effect;
})();
