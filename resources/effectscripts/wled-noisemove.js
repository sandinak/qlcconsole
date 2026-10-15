/*
  QLC+ Effect Script: WLED Noisemove
  Sixteen dots, one for each band of the music from bass to treble, each wandering
  the strip on its own smooth noise in its own colour and as bright as its band,
  trails fading; speed is how fast they wander, intensity how fast the trails
  fade.
  Written from what WLED's "Noisemove" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Noisemove";
    effect.description = "From WLED: a dot for each band, wandering on noise";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Noisemove";
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
        WLED.fadeOut(seg, 0.3 + seg.intensity / 255 * 0.69, dt);
        if (au) {
            var s = seg.now / 1000 * (0.2 + seg.speed / 255 * 2);
            for (var b = 0; b < 16; b++) {
                var p = Math.round(Math.max(0, Math.min(1, (WLED.noise(s, 360 + b) - 0.1) / 0.8)) * (len - 1));
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(WLED.hue(b * 16), au.bands[b] * 1.5)));
            }
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
