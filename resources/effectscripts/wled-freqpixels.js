/*
  QLC+ Effect Script: WLED Freqpixels
  Pixels lit at random, each the colour of the loudest pitch and as bright as it,
  fading away; speed is how fast they fade, intensity how many.
  Written from what WLED's "Freqpixels" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Freqpixels";
    effect.description = "From WLED: random pixels in the colour of the pitch";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Freqpixels";
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
        WLED.fadeOut(seg, 0.4 + seg.speed / 255 * 0.59, dt);
        if (au && au.mag > 0.02) {
            var count = 1 + Math.floor(seg.intensity / 255 * len / 8);
            var c = WLED.dim(WLED.hue(Math.floor(au.pitch * 255)), au.mag * 1.5);
            for (var k = 0; k < count; k++)
                seg.setPixelColor(Math.floor(WLED.hash(seg.now / 1000 + k * 0.37, 340) * len), c);
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
