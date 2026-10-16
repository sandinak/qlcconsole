/*
  QLC+ Effect Script: WLED Noisemeter
  A VU meter of noise: from the start of the strip as far as the music is loud, in
  drifting noise colours, fading back as it quietens; speed is how fast it fades,
  intensity how far it reaches.
  Written from what WLED's "Noisemeter" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noisemeter";
    effect.description = "From WLED: a VU meter of drifting noise";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noisemeter";
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
            var reach = Math.min(len, au.vol * len * (0.5 + seg.intensity / 255 * 2));
            for (var i = 0; i < reach; i++)
                seg.setPixelColor(i, seg.randomColor(WLED.noise(i * 0.13 + seg.now / 600, 330)));
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
