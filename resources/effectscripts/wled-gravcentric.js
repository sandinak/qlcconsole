/*
  QLC+ Effect Script: WLED Gravcentric
  A VU meter from the middle of the strip out both ways, as wide as the music is
  loud, its colour picked off the palette by the volume as the palette slowly
  turns; ends fall back slowly. Speed is how fast they fall, intensity the
  sensitivity.
  Written from what WLED's "Gravcentric" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Gravcentric";
    effect.description = "From WLED: a VU meter from the middle, coloured by the volume";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Gravcentric";
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
        var half = vol * len / 2, mid = (len - 1) / 2;
        state.top = Math.max(half, (state.top || 0) - dt / 1000 * len / 2 * (0.2 + seg.speed / 255 * 1.8));
        for (var i = 0; i < len; i++) {
            var d = Math.abs(i - mid);
            var h = vol * 0.8 + d / len * 0.2 + seg.now / 10000;
            if (d <= half) seg.setPixelColor(i, seg.randomColor(h - Math.floor(h)));
        }
        if (state.top > 0.5) {
            seg.setPixelColor(Math.round(mid - state.top), 0x808080);
            seg.setPixelColor(Math.round(mid + state.top), 0x808080);
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
