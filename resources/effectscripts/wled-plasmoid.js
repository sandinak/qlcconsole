/*
  QLC+ Effect Script: WLED Plasmoid
  A plasma of sine waves rolling over each other along the strip, lit as brightly
  as the music is loud; speed is how fast the waves roll, intensity how many
  pixels are lit.
  Written from what WLED's "Plasmoid" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Plasmoid";
    effect.description = "From WLED: sine-wave plasma lit by the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Plasmoid";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.dataChannels = ["audio"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var au = WLED.audio(data);
        seg.fill(0);
        if (au) {
            var ph = seg.now / 1000 * (0.5 + seg.speed / 255 * 3);
            var cut = 1 - seg.intensity / 255 * 2;
            for (var i = 0; i < len; i++) {
                var v = Math.sin(i * 0.3 + ph) * 0.5 + Math.sin(i * 0.17 - ph * 1.3) * 0.5;
                if (v < cut) continue;
                var h = (v + 1) / 2 * 0.8 + seg.now / 30000;
                seg.setPixelColor(i, WLED.dim(seg.randomColor(h - Math.floor(h)), (v + 1) / 2 * au.vol * 2));
            }
        }
        return seg.out();
    };

    return effect;
})();
