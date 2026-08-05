/*
  QLC+ Effect Script: MIDI Keys
  Play a MIDI controller and the fixtures light up like keys — the note range
  is tiled across the fixtures, each lit at its note's brightness and fading on
  release (sustain-pedal aware). Port of the WLED "MIDI Keys" idea.

  Subscribes to the host "midi" data channel (data.midi.velocity[]/held[]/
  sustain). Colour comes from the look's Colour palette (by position).
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "MIDI Keys";
    effect.description = "Fixtures light as keys when you play MIDI notes";
    effect.author      = "WLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["midi"];
    effect.notes = "Tiles the note range [Note low..Note high] across the fixtures: each note lights its band and fades on release. Hold the sustain pedal (CC64) for a slower fade. Colours come from the look's stacked Colour palettes, spread by position. Set Note low/high to match your controller.";

    effect.parameters = [
        { name: "midiSource",  description: "MIDI source (0 = any; else universe # from Inputs/Outputs)", min: 0, max: 16, defaultValue: 0 },
        { name: "autoRange",   description: "Range mode",               defaultValue: 1, values: ["Manual (use Note low/high)", "Learn — play your lowest & highest key"] },
        { name: "noteLow",     description: "Lowest MIDI note (Manual mode)",  min: 0, max: 127, defaultValue: 36 },
        { name: "noteHigh",    description: "Highest MIDI note (Manual mode)", min: 0, max: 127, defaultValue: 84 },
        { name: "axis",        description: "Note spread axis",         defaultValue: 0, values: ["Horizontal (columns)", "Vertical (rows)", "Auto (wider)"] },
        { name: "release",     description: "Release fade (seconds)",   min: 0.05, max: 5.0,  defaultValue: 0.6 },
        { name: "sustainFade", description: "Sustained fade (seconds)", min: 0.2,  max: 20.0, defaultValue: 6.0 },
        { name: "velFloor",    description: "Min brightness for held notes (0-31)", min: 0, max: 31, defaultValue: 0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var m   = data && data.midi;
        // Lock onto one device by universe #, or 0 = any (merged).
        var src = params.midiSource | 0;
        if (m && src > 0 && m.universes && m.universes[src]) m = m.universes[src];
        var lo  = params.noteLow  | 0;
        var hi  = params.noteHigh | 0;
        // Learn mode: remember the lowest & highest notes ever played and use
        // them as the range — just play your lowest key then your highest.
        if ((params.autoRange | 0) === 1) {
            if (state.learnLo === undefined) { state.learnLo = 127; state.learnHi = 0; }
            var lh = m && m.held;
            if (lh) for (var ln = 0; ln < 128; ln++) if (lh[ln]) {
                if (ln < state.learnLo) state.learnLo = ln;
                if (ln > state.learnHi) state.learnHi = ln;
            }
            if (state.learnHi > state.learnLo) { lo = state.learnLo; hi = state.learnHi; }
        }
        if (hi <= lo) hi = lo + 1;

        // Per-note level (0..1), snapped up on strike, decayed on release.
        if (!state.lvl) {
            state.lvl = [];
            for (var k = 0; k < 128; k++) state.lvl[k] = 0;
            state.t = inputs._time || 0;
        }
        var dt = Math.max(0, (inputs._time || 0) - state.t);
        state.t = inputs._time || 0;

        var sustain = m ? m.sustain : false;
        var relRate = 1 / Math.max(0.02, params.release || 0.6);
        var susRate = 1 / Math.max(0.05, params.sustainFade || 6);
        var floor   = (params.velFloor || 0) / 31;

        var vel = m && m.velocity, held = m && m.held;
        for (var note = lo; note <= hi; note++) {
            var h = held ? held[note] : false;
            var v = vel  ? vel[note] / 127 : 0;
            if (h) {
                if (v > state.lvl[note]) state.lvl[note] = v;   // softer retrigger doesn't dim
                if (state.lvl[note] < floor) state.lvl[note] = floor;
            } else {
                var r = sustain ? susRate : relRate;
                state.lvl[note] = Math.max(0, state.lvl[note] - r * dt);
            }
        }

        var lk   = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var span = hi - lo;

        // Spread the keyboard across the WIDER grid axis (columns for a wide grid,
        // rows for a tall one) — the other axis mirrors the same note as a "key
        // bar". A 1-D strip (rows=1) just maps note → fixture as before.
        var g0 = fixtures[0].grid || { cols: n, rows: 1 };
        var cols = g0.cols || n, rows = g0.rows || 1;
        var ax = params.axis | 0;   // 0=Horizontal, 1=Vertical, 2=Auto
        var horizontal = (ax === 1) ? false : (ax === 2) ? (cols >= rows) : true;
        var axisLen = Math.max(1, horizontal ? cols : rows);

        return fixtures.map(function(f, i) {
            var g = f.grid || { col: i, row: 0, cols: cols, rows: rows };
            var axisPos = horizontal ? g.col : g.row;
            // This cell's note band → brightest note in it.
            var a = lo + Math.floor(axisPos       / axisLen * (span + 1));
            var b = lo + Math.floor((axisPos + 1) / axisLen * (span + 1));
            if (b <= a) b = a + 1;
            var mx = 0;
            for (var note = a; note < b && note <= hi; note++)
                if (state.lvl[note] > mx) mx = state.lvl[note];

            var pos = axisLen > 1 ? axisPos / (axisLen - 1) : 0;
            var col = lk.length ? lk[Math.min(lk.length - 1, Math.floor(pos * lk.length))]
                                : { r: 255, g: 255, b: 255 };
            var out = { r: Math.round(col.r * mx), g: Math.round(col.g * mx), b: Math.round(col.b * mx) };
            if (f.hasDimmer) out.dimmer = mx;
            return out;
        });
    };

    return effect;
})()
