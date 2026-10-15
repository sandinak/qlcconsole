/*
  QLC+ Effect Script: WLED Noisefire
  Fire on the music: flames of noise licking up from the start of the strip, as
  high and hot as the music is loud; speed is how fast they flicker, intensity how
  hot.
  Written from what WLED's "Noisefire" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noisefire";
    effect.description = "From WLED: noise flames as hot as the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noisefire";
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
            var hot = Math.min(1, au.vol * (0.5 + seg.intensity / 255 * 3));
            var s = seg.now / 1000 * (0.5 + seg.speed / 255 * 3);
            for (var i = 0; i < len; i++) {
                var heat = WLED.noise(i * 0.2 - s, 350) * (1 - i / len * 0.8) * hot;
                seg.setPixelColor(i, seg.palette ? WLED.dim(seg.colorFromPalette(Math.floor(heat * 240), false, false, 0), heat * 2)
                                                 : WLED.heatColor(heat));
            }
        }
        return seg.out();
    };

    return effect;
})();
