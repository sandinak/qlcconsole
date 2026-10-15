/*
  QLC+ Effect Script: WLED Gravimeter
  A VU meter with gravity: a bar from the start as long as the music is loud,
  coloured by drifting noise, its top marked by a dot that falls back slowly;
  speed is how fast the dot falls, intensity the sensitivity.
  Written from what WLED's "Gravimeter" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Gravimeter";
    effect.description = "From WLED: a VU meter whose top falls back slowly";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Gravimeter";
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
        seg.fill(0);
        var vol = au ? Math.min(1, au.vol * (0.5 + seg.intensity / 255 * 2.5)) : 0;
        var bar = vol * len;
        state.top = Math.max(bar, (state.top || 0) - dt / 1000 * len * (0.2 + seg.speed / 255 * 1.8));
        for (var i = 0; i < bar; i++)
            seg.setPixelColor(i, seg.randomColor(WLED.noise(i * 0.2 + seg.now / 1000, 300)));
        var top = Math.min(len - 1, Math.floor(state.top));
        if (state.top > 0.5) seg.setPixelColor(top, paint(top));
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
