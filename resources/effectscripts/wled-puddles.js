/*
  QLC+ Effect Script: WLED Puddles
  Puddles of colour splashed at random places as the music plays, bigger and
  brighter the louder it is, each fading away; speed is how fast they fade,
  intensity how big.
  Written from what WLED's "Puddles" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Puddles";
    effect.description = "From WLED: puddles of colour splashed by the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Puddles";
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
        WLED.fadeOut(seg, 0.3 + seg.speed / 255 * 0.69, dt);
        if (au && au.vol > 0.05 && (state.last === undefined || seg.now - state.last >= 60 || seg.now < state.last)) {
            state.last = seg.now;
            var size = Math.max(1, Math.round((1 + seg.intensity / 255 * len / 8) * Math.min(1, au.vol * 2)));
            var pos = Math.floor(WLED.hash(seg.now, 310) * len);
            var c = WLED.dim(seg.randomColor(WLED.hash(seg.now, 311)), au.vol * 1.5);
            for (var j = 0; j < size; j++) seg.setPixelColor(pos + j, c);
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
