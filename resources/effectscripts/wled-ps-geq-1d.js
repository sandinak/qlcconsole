/*
  QLC+ Effect Script: WLED PS GEQ 1D
  An equaliser of particles: each band of the music, bass to treble, throws
  particles of its own colour up from the start of the strip, higher the louder it
  is, to fall back; speed is how hard they are thrown, intensity how many.
  Written from what WLED's "PS GEQ 1D" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS GEQ 1D";
    effect.description = "From WLED: particles thrown up by each band of the music";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS GEQ 1D";
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
        var dt = WLED.recall(seg, state) / 1000;
        seg.fill(0);
        var parts = state.parts || [];
        var g = len * 1.5;
        for (var k = 0; k < parts.length; k++) { parts[k].v -= g * dt; parts[k].x += parts[k].v * dt; }
        parts = parts.filter(function(p) { return p.x >= 0; });
        if (au) {
            for (var b = 0; b < 16 && parts.length < 96; b++) {
                var lvl = au.bands[b];
                if (lvl > 0.15 && WLED.hash(seg.now / 1000 + b * 0.71, 390) < lvl * (0.2 + seg.intensity / 255 * 0.8))
                    parts.push({ x: 0, v: Math.sqrt(2 * g * len) * (0.3 + 0.7 * seg.speed / 255) * lvl, b: b });
            }
        }
        for (var j = 0; j < parts.length; j++) {
            var i = Math.round(parts[j].x);
            seg.setPixelColor(i, WLED.add(seg.getPixelColor(i), WLED.hue(parts[j].b * 16)));
        }
        state.parts = parts;
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
