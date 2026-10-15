/*
  QLC+ Effect Script: WLED DJ Light
  Light sent out from the middle of the strip to its ends, each new pixel mixed
  from the music: bass as red, mids as green, treble as blue; speed is how fast it
  travels.
  Written from what WLED's "DJ Light" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED DJ Light";
    effect.description = "From WLED: bass, mids and treble as colour, out from the middle";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "DJ Light";
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
        var n = Math.min(len, WLED.steps(state, dt, WLED.period(seg.speed, 120, 8)));
        for (var s = 0; s < n; s++) WLED.shift(seg, 0);
        var c = 0;
        if (au) {
            var avg = function(a, b) { var t = 0; for (var k = a; k < b; k++) t += au.bands[k]; return t / (b - a); };
            var ch = function(v) { return Math.min(255, Math.round(v * 1.5 * 255)); };
            c = WLED.rgb(ch(avg(0, 5)), ch(avg(5, 10)), ch(avg(10, 16)));
        }
        var mid = Math.floor((len - 1) / 2);
        seg.setPixelColor(mid, c);
        seg.setPixelColor(mid + 1, c);
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
