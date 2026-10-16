/*
  QLC+ Effect Script: WLED Matripix
  Like Matrix: the strip runs toward its start, a new pixel coming in at the end
  as bright as the music is loud, its colour slowly turning; intensity is how
  bright.
  Written from what WLED's "Matripix" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Matripix";
    effect.description = "From WLED: pixels raining along, as bright as the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Matripix";
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
        var n = Math.min(len, WLED.steps(state, dt, WLED.period(seg.speed, 200, 10)));
        for (var s = 0; s < n; s++) WLED.shift(seg, -1);
        var h = seg.now / 10000;
        seg.setPixelColor(len - 1, au ? WLED.dim(seg.randomColor(h - Math.floor(h)), au.vol * (0.5 + seg.intensity / 255 * 1.5)) : 0);
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
