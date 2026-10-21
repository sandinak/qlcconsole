/*
  QLC+ Effect Script: WLED PS Sonic Boom
  Each beat of the music sets off a burst of particles that fly apart and fade,
  each burst a little further along the strip and a step further round the colour
  wheel; speed is how fast they fly, intensity how big the burst.
  Written from what WLED's "PS Sonic Boom" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Sonic Boom";
    effect.description = "From WLED: bursts of particles on the beat";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Sonic Boom";
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
        for (var k = 0; k < parts.length; k++) {
            parts[k].x += parts[k].v * dt;
            parts[k].v *= Math.pow(0.3, dt);
            parts[k].age += dt;
        }
        parts = parts.filter(function(p) { return p.age < 1 && p.x >= 0 && p.x < len; });
        if (WLED.beat(state, au, seg.now)) {
            state.pos = ((state.pos === undefined ? WLED.hash(seg.now, 401) * len : state.pos) + len * 0.23) % len;
            state.hue = ((state.hue === undefined ? Math.floor(au.pitch * 255) : state.hue) + 24) & 255;
            var n = 6 + Math.floor(seg.intensity / 255 * 20);
            for (var b = 0; b < n && parts.length < 160; b++) {
                var r = WLED.hash(seg.now / 1000 + b * 0.13, 400);
                parts.push({ x: state.pos, v: (r * 2 - 1) * len * (0.3 + seg.speed / 255 * 1.2), age: 0, h: state.hue });
            }
        }
        for (var j = 0; j < parts.length; j++) {
            var i = Math.floor(parts[j].x);
            seg.setPixelColor(i, WLED.add(seg.getPixelColor(i), WLED.dim(WLED.hue(parts[j].h), 1 - parts[j].age)));
        }
        state.parts = parts;
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
