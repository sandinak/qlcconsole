/*
  QLC+ Effect Script: WLED Juggles
  Juggling balls to the music: dots swinging up and down the strip at their own
  rates, as bright as the music is loud, leaving fading trails; intensity is how
  many.
  Written from what WLED's "Juggles" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Juggles";
    effect.description = "From WLED: juggling balls, as bright as the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Juggles";
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
        WLED.fadeOut(seg, 0.95, dt);
        if (au) {
            var balls = 1 + Math.floor(seg.intensity / 32);
            for (var k = 0; k < balls; k++) {
                var p = WLED.beatsin16(Math.round(6 + seg.speed / 24 + k * 3), 0, len - 1, seg.now);
                var h = k / balls + seg.now / 20000;
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(seg.randomColor(h - Math.floor(h)), au.vol * 1.5)));
            }
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
