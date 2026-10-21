/*
  QLC+ Effect Script: WLED Puddlepeak
  Puddles of colour splashed at random places on each beat of the music, fading
  away between; speed is how fast they fade, intensity how big.
  Written from what WLED's "Puddlepeak" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Puddlepeak";
    effect.description = "From WLED: puddles of colour splashed on the beat";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Puddlepeak";
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
        if (WLED.beat(state, au, seg.now)) {
            var size = Math.max(1, Math.round(1 + seg.intensity / 255 * len / 8));
            var pos = Math.floor(WLED.hash(seg.now, 312) * len);
            var c = seg.randomColor(WLED.hash(seg.now, 313));
            for (var j = 0; j < size; j++) seg.setPixelColor(pos + j, c);
        }
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
