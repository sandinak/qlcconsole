/*
  QLC+ Effect Script: WLED PS Sonic Stream
  A stream of particles flowing along the strip, let in at its start while the
  bass plays and as bright as it, each a step further round the colour wheel;
  speed is how fast it flows, intensity how readily it lets them in.
  Written from what WLED's "PS Sonic Stream" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Sonic Stream";
    effect.description = "From WLED: a stream of particles let in by the bass";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Sonic Stream";
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
        var v = len * (0.3 + seg.speed / 255 * 1.5);
        for (var k = 0; k < parts.length; k++) parts[k].x += v * dt;
        parts = parts.filter(function(p) { return p.x < len; });
        if (au) {
            var bass = (au.bands[0] + au.bands[1] + au.bands[2] + au.bands[3]) / 4;
            var gap = state.lastEmit === undefined ? 1e9 : seg.now - state.lastEmit;
            if (bass > 0.6 - seg.intensity / 255 * 0.55 && (gap >= 30 || gap < 0) && parts.length < 128) {
                state.lastEmit = seg.now;
                state.hue = ((state.hue === undefined ? Math.floor(au.pitch * 255) : state.hue) + 8) & 255;
                parts.push({ x: 0, h: state.hue, l: Math.min(1, bass * 1.5) });
            }
        }
        for (var j = 0; j < parts.length; j++) {
            var i = Math.floor(parts[j].x);
            seg.setPixelColor(i, WLED.add(seg.getPixelColor(i), WLED.dim(WLED.hue(parts[j].h), parts[j].l)));
        }
        state.parts = parts;
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
