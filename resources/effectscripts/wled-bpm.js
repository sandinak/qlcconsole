/*
  QLC+ Effect Script: WLED Bpm
  The palette laid along the strip, pulsing in brightness at the effect's beat; speed
  is the tempo.
  Written from what WLED's "Bpm" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Bpm";
    effect.description = "From WLED: palette pulsing to a tempo";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Bpm";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var bpm = 30 + seg.speed / 255 * 150;
        var beat = seg.now / (60000 / bpm);
        var pulse = 0.35 + 0.65 * (0.5 + 0.5 * Math.cos((beat % 1) * 2 * Math.PI));
        for (var i = 0; i < len; i++) {
            var c = (seg.palette ? seg.colorFromPalette(Math.floor((i * 2 + beat * 8) % 256), false, true, 0) : WLED.hue(Math.floor((i * 2 + beat * 8) % 256)));
            seg.setPixelColor(i, WLED.colorBlend(0, c, Math.round(Math.min(1, pulse + i / len * 0.2) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
