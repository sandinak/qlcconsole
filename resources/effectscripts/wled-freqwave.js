/*
  QLC+ Effect Script: WLED Freqwave
  The loudest pitch as a colour, low notes red round to high notes violet, sent
  out from the middle of the strip in waves; speed is how fast they spread.
  Written from what WLED's "Freqwave" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Freqwave";
    effect.description = "From WLED: the pitch as colour, sent out from the middle";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Freqwave";
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
        var n = Math.min(len, WLED.steps(state, dt, WLED.period(seg.speed, 100, 8)));
        for (var s = 0; s < n; s++) WLED.shift(seg, 0);
        var c = au ? WLED.dim(WLED.hue(Math.floor(au.pitch * 255)), au.mag * 1.5) : 0;
        var mid = Math.floor((len - 1) / 2);
        seg.setPixelColor(mid, c);
        seg.setPixelColor(mid + 1, c);
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
