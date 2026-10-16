/*
  QLC+ Effect Script: WLED Waterfall
  A waterfall of the music: the strip runs toward its start, each new pixel the
  colour of the loudest pitch and as bright as it, a flash of colour 2 on each
  beat; speed is how fast it runs, intensity shifts the colours.
  Written from what WLED's "Waterfall" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Waterfall";
    effect.description = "From WLED: pitches falling down the strip, flashing on the beat";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Waterfall";
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
        var n = Math.min(len, WLED.steps(state, dt, WLED.period(seg.speed, 150, 10)));
        for (var s = 0; s < n; s++) WLED.shift(seg, -1);
        var c = 0;
        if (au) {
            if (WLED.beat(state, au, seg.now)) c = seg.color(1) ? seg.color(1) : 0xFFFFFF;
            else c = WLED.dim(WLED.hue((Math.floor(au.pitch * 255) + seg.intensity) & 255), au.mag * 1.5);
        }
        seg.setPixelColor(len - 1, c);
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
