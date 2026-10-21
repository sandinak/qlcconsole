/*
  QLC+ Effect Script: WLED Phased Noise
  Noisy sine waves: waves of the palette (or colour 1) rolling along, roughened by
  noise; intensity is how many waves.
  Written from what WLED's "Phased Noise" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Phased Noise";
    effect.description = "From WLED: noisy sine waves";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Phased Noise";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 6000, 400) * 2 * Math.PI;
        var freq = 0.1 + seg.intensity / 255 * 0.6;
        for (var i = 0; i < len; i++) {
            var wave = 0.5 + 0.5 * Math.sin(i * freq + s);
            var n = WLED.noise(i * 0.15 + seg.now / 700, 150);
            seg.setPixelColor(i, WLED.mix(seg.color(1), paint(i), wave + (n - 0.5) * 0.7));
        }
        return seg.out();
    };

    return effect;
})();
