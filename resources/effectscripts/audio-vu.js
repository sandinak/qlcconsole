/*
  QLC+ Effect Script: Audio VU
  The whole rig pulses with the overall audio level — a big, simple beat/volume
  reaction. Optionally splits bass/mid/high across the look's colours.

  Subscribes to the host "audio" data channel. Requires an audio input assigned
  in Inputs/Outputs.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Audio VU";
    effect.description = "Whole-rig pulse driven by the audio level";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["audio"];
    effect.notes = "The rig brightness follows the audio level with a fast attack and an eased release, like a VU meter. Gain scales it; Release sets the fall time. Set Start/End Hz to react to just one part of the spectrum — e.g. 40–150 Hz for a kick drum, 3000–5000 Hz for cymbals. Bass-tilt mixes the look's first colour on lows and later colours on highs. Requires an audio input in Inputs/Outputs.";

    effect.parameters = [
        { name: "gain",     description: "Response gain",       min: 0.2, max: 6.0, defaultValue: 1.8 },
        { name: "release",  description: "Release (seconds)",   min: 0.02, max: 2.0, defaultValue: 0.25 },
        { name: "startHz",  description: "Focus: start Hz (0 = lowest)",  min: 0, max: 5000, defaultValue: 0 },
        { name: "endHz",    description: "Focus: end Hz (5000 = highest)", min: 0, max: 5000, defaultValue: 5000 },
        { name: "bassTilt", description: "Colour by band",      defaultValue: 1, values: ["Off", "On"] }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var a     = data && data.audio;
        var level = a ? (a.level || 0) : 0;
        var bands = (a && a.bands) ? a.bands : [];
        var gain  = params.gain || 1.8;

        // Focus band range from the Hz window (bands tile 0..maxHz linearly).
        var maxHz = (a && a.maxHz) ? a.maxHz : 5000;
        var N     = bands.length;
        var startHz = Math.max(0, params.startHz | 0);
        var endHz   = params.endHz ? (params.endHz | 0) : maxHz;
        if (endHz <= startHz) endHz = maxHz;
        var full  = (startHz <= 0 && endHz >= maxHz);
        var bLo   = N ? Math.max(0, Math.floor(startHz / maxHz * N)) : 0;
        var bHi   = N ? Math.min(N, Math.ceil(endHz / maxHz * N)) : 0;
        if (bHi <= bLo) bHi = Math.min(N, bLo + 1);
        // Level source: overall RMS for the full band, else the loudest focused band.
        if (!full && N) {
            var mx = 0;
            for (var bi = bLo; bi < bHi; bi++) if (bands[bi] > mx) mx = bands[bi];
            level = mx;
        }

        if (state.v === undefined) { state.v = 0; state.t = inputs._time || 0; }
        var dt = Math.max(0, (inputs._time || 0) - state.t);
        state.t = inputs._time || 0;

        var target = Math.min(1, level * gain);
        if (target > state.v) state.v = target;                 // fast attack
        else state.v = Math.max(target, state.v - dt / Math.max(0.02, params.release || 0.25));

        // Colour: bass/mid/high energy mixed onto the look's colours, else white.
        var lk = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var col = { r: 255, g: 255, b: 255 };
        if (((params.bassTilt | 0) === 1) && lk.length && bands.length) {
            function bandAvg(a0, a1) { var s = 0, c = 0; for (var i = a0; i < a1 && i < bands.length; i++){ s += bands[i]; c++; } return c ? s / c : 0; }
            // Split the FOCUSED range (low→high) into thirds for the colour tilt.
            var span = Math.max(3, bHi - bLo);
            var third = Math.max(1, Math.floor(span / 3));
            var e = [bandAvg(bLo, bLo + third), bandAvg(bLo + third, bLo + 2 * third), bandAvg(bLo + 2 * third, bHi)];
            var r = 0, g = 0, b = 0, wsum = 0;
            for (var k = 0; k < 3; k++) {
                var c2 = lk[Math.min(lk.length - 1, k)];
                var w = e[k] + 0.001;
                r += c2.r * w; g += c2.g * w; b += c2.b * w; wsum += w;
            }
            col = { r: r / wsum, g: g / wsum, b: b / wsum };
        } else if (lk.length && lk[0].r !== undefined) {
            col = lk[0];
        }

        var bv = state.v;
        return fixtures.map(function(f) {
            var out = { r: Math.round(col.r * bv), g: Math.round(col.g * bv), b: Math.round(col.b * bv) };
            if (f.hasDimmer) out.dimmer = bv;
            return out;
        });
    };

    return effect;
})()
