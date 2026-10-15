/*
  QLC+ Effect Script: WLED Rocktaves
  The note of the music as a colour -- the same colour for the same note in any
  octave -- on a dot swinging back and forth along the strip, faster for higher
  octaves, trails fading.
  Written from what WLED's "Rocktaves" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Rocktaves";
    effect.description = "From WLED: the note as a colour, swinging back and forth";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Rocktaves";
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
        WLED.fadeOut(seg, 0.9, dt);
        if (au && au.mag > 0.05) {
            var oct = Math.log(Math.max(55, au.majorHz) / 55) / Math.LN2;
            var note = oct - Math.floor(oct);
            var u = 0.5 + 0.5 * Math.sin(seg.now / 1000 * 2 * Math.PI * (0.2 + Math.floor(oct) * 0.1));
            var p = Math.round(u * (len - 1));
            seg.setPixelColor(p, WLED.dim(WLED.hue(Math.floor(note * 255)), au.mag * 1.5));
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
