/*
  QLC+ Effect Script: WLED Gravcenter
  A VU meter from the middle of the strip out both ways, as wide as the music is
  loud, coloured by drifting noise, its ends marked by dots that fall back slowly;
  speed is how fast the dots fall, intensity the sensitivity.
  Written from what WLED's "Gravcenter" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Gravcenter";
    effect.description = "From WLED: a VU meter from the middle, ends falling back";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Gravcenter";
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
            if (d <= half) seg.setPixelColor(i, seg.randomColor(WLED.noise(d * 0.2 + seg.now / 1000, 380)));
        }
        if (state.top > 0.5) {
            seg.setPixelColor(Math.round(mid - state.top), 0xC0C0C0);
            seg.setPixelColor(Math.round(mid + state.top), 0xC0C0C0);
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
