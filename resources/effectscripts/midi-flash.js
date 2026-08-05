/*
  QLC+ Effect Script: MIDI Flash
  The whole rig flashes on the loudest currently-played note and fades out —
  retriggering any note pulls it back up. Port of the WLED "MIDI Flash" idea.

  Subscribes to the host "midi" data channel. Colour from the look's first
  Colour palette.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "MIDI Flash";
    effect.description = "Whole-rig flash driven by the loudest played note";
    effect.author      = "WLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["midi"];
    effect.notes = "The whole rig flashes to the loudest currently-held note and fades out; playing any note pulls it back up. Sustain pedal (CC64) slows the fade. Colour comes from the look's first Colour palette.";

    effect.parameters = [
        { name: "midiSource",  description: "MIDI source (0 = any; else universe # from Inputs/Outputs)", min: 0, max: 16, defaultValue: 0 },
        { name: "release",     description: "Release fade (seconds)",   min: 0.03, max: 4.0,  defaultValue: 0.4 },
        { name: "sustainFade", description: "Sustained fade (seconds)", min: 0.2,  max: 20.0, defaultValue: 6.0 },
        { name: "velFloor",    description: "Min brightness for held notes (0-31)", min: 0, max: 31, defaultValue: 0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var m = data && data.midi;
        var src = params.midiSource | 0;
        if (m && src > 0 && m.universes && m.universes[src]) m = m.universes[src];
        if (state.f === undefined) { state.f = 0; state.t = inputs._time || 0; }
        var dt = Math.max(0, (inputs._time || 0) - state.t);
        state.t = inputs._time || 0;

        // Loudest currently-held note → snap the flash up to it.
        var floor  = (params.velFloor || 0) / 31;
        var maxHeld = 0, anyHeld = false;
        var vel = m && m.velocity, held = m && m.held;
        if (vel && held) {
            for (var note = 0; note < 128; note++) {
                if (held[note]) {
                    anyHeld = true;
                    var v = Math.max(vel[note] / 127, floor);
                    if (v > maxHeld) maxHeld = v;
                }
            }
        }
        if (maxHeld > state.f) state.f = maxHeld;   // instant attack / retrigger

        // Decay (slower while sustain held).
        var sustain = m ? m.sustain : false;
        var rate = 1 / Math.max(sustain ? 0.2 : 0.02,
                                sustain ? (params.sustainFade || 6) : (params.release || 0.4));
        if (!anyHeld || state.f > maxHeld)
            state.f = Math.max(anyHeld ? maxHeld : 0, state.f - rate * dt);

        var b  = state.f;
        var lk = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var col = (lk.length && lk[0].r !== undefined) ? lk[0] : { r: 255, g: 255, b: 255 };
        return fixtures.map(function(f) {
            var out = { r: Math.round(col.r * b), g: Math.round(col.g * b), b: Math.round(col.b * b) };
            if (f.hasDimmer) out.dimmer = b;
            return out;
        });
    };

    return effect;
})()
