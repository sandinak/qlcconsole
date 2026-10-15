/*
  QLC+ Effect Script: WLED Midnoise
  Noise spreading out from the middle of the strip as far as the music is loud,
  and fading as it falls back; speed is how fast it fades, intensity how far it
  can reach.
  Written from what WLED's "Midnoise" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Midnoise";
    effect.description = "From WLED: noise from the middle, as wide as the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Midnoise";
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
        var dt = WLED.recall(seg, state);
        WLED.fadeOut(seg, 0.4 + seg.speed / 255 * 0.59, dt);
        if (au) {
            var reach = Math.min(len / 2, au.vol * 2 * len / 2 * seg.intensity / 255);
            var mid = (len - 1) / 2;
            for (var i = 0; i < len; i++)
                if (Math.abs(i - mid) <= reach)
                    seg.setPixelColor(i, seg.randomColor(WLED.noise(i * 0.15 + seg.now / 800, 320)));
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
