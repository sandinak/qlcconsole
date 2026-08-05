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
    effect.notes = "The rig brightness follows the audio level with a fast attack and an eased release, like a VU meter. Gain scales it; Release sets the fall time. Bass-tilt mixes the look's first colour on lows and later colours on highs. Requires an audio input in Inputs/Outputs.";

    effect.parameters = [
        { name: "gain",     description: "Response gain",       min: 0.2, max: 6.0, defaultValue: 1.8 },
        { name: "release",  description: "Release (seconds)",   min: 0.02, max: 2.0, defaultValue: 0.25 },
        { name: "bassTilt", description: "Colour by band",      defaultValue: 1, values: ["Off", "On"] }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var a     = data && data.audio;
        var level = a ? (a.level || 0) : 0;
        var bands = (a && a.bands) ? a.bands : [];
        var gain  = params.gain || 1.8;

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
            var third = Math.max(1, Math.floor(bands.length / 3));
            var e = [bandAvg(0, third), bandAvg(third, 2 * third), bandAvg(2 * third, bands.length)];
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
