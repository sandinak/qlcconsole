/*
  QLC+ Effect Script: WLED Freqmap
  The loudest pitch of the music shown as a place on the strip, low notes near the
  start and high ones near the end, lit in a colour for the pitch and fading;
  speed is how fast it fades, intensity the starting colour.
  Written from what WLED's "Freqmap" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Freqmap";
    effect.description = "From WLED: the pitch shown as a place on the strip";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Freqmap";
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
            var p = Math.round(au.pitch * (len - 1));
            seg.setPixelColor(p, WLED.dim(WLED.hue((Math.floor(au.pitch * 240) + seg.intensity) & 255), au.mag * 1.5));
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
