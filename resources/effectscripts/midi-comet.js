/*
  QLC+ Effect Script: MIDI Comet
  Every struck note launches a comet from that note's position across the pixel
  grid. Direction is melodic, not positional: it's the reverse of the interval
  from the previous note — play higher and the comet flies "down" the grid, play
  lower and it flies "up". Port of the WLED "MIDI Comet" idea.

  Subscribes to the host "midi" data channel. Colour from the look's palette.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "MIDI Comet";
    effect.description = "Each note fires a comet; direction follows the melody";
    effect.author      = "WLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["midi"];
    effect.notes = "Each note strike launches a comet from that note's position along the fixture grid (or strip). Direction is the reverse of the melodic interval: a higher note than the last flies one way, a lower note the other. Speed sets travel; Tail the trail length. Colours from the look's palette.";

    effect.parameters = [
        { name: "midiSource", description: "MIDI source (0 = any; else universe # from Inputs/Outputs)", min: 0, max: 16, defaultValue: 0 },
        { name: "autoRange", description: "Range (use the \"Learn range…\" button to auto-set)", defaultValue: 0, values: ["Manual (use Note low/high)", "Live-follow (tracks played notes)"] },
        { name: "noteLow",  description: "Lowest MIDI note (Manual mode)",  min: 0, max: 127, defaultValue: 36 },
        { name: "noteHigh", description: "Highest MIDI note (Manual mode)", min: 0, max: 127, defaultValue: 84 },
        { name: "axis",     description: "Note spread axis", defaultValue: 0, values: ["Horizontal (columns)", "Vertical (rows)", "Auto (wider)"] },
        { name: "speed",    description: "Comet speed (cells/sec)", min: 2, max: 80, defaultValue: 24 },
        { name: "tail",     description: "Tail length (cells)",     min: 2, max: 24, defaultValue: 6 }
    ];

    var MAX = 16;   // comet pool

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var g0    = fixtures[0].grid || { cols: n, rows: 1 };
        var cols  = g0.cols || n, rows = g0.rows || 1;
        // Notes/comets travel along the chosen grid axis (default Horizontal =
        // columns); the other axis mirrors as bars.
        var ax = params.axis | 0;   // 0=Horizontal, 1=Vertical, 2=Auto
        var horizontal = (ax === 1) ? false : (ax === 2) ? (cols >= rows) : true;
        var axisLen = Math.max(1, horizontal ? cols : rows);

        var m  = data && data.midi;
        var src = params.midiSource | 0;
        if (m && src > 0 && m.universes && m.universes[src]) m = m.universes[src];
        var lo = params.noteLow | 0, hi = params.noteHigh | 0;
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

        if (!state.comets) {
            state.comets = [];
            state.prevHeld = [];
            for (var k = 0; k < 128; k++) state.prevHeld[k] = false;
            state.lastNote = -1;
            state.t = inputs._time || 0;
        }
        var t  = inputs._time || 0;
        var dt = Math.max(0, t - state.t);
        state.t = t;

        var vel = m && m.velocity, held = m && m.held;

        // Fresh strikes → spawn comets. Direction = reverse of the melodic move.
        for (var note = lo; note <= hi; note++) {
            var h = held ? held[note] : false;
            if (h && !state.prevHeld[note]) {
                var pos = lo === hi ? 0 : Math.round((note - lo) / (hi - lo) * (axisLen - 1));
                var dir = -1;
                if (state.lastNote >= 0) {
                    if (note > state.lastNote)      dir = -1;   // higher → fly down
                    else if (note < state.lastNote) dir = 1;    // lower  → fly up
                }
                state.lastNote = note;
                var v = vel ? vel[note] / 127 : 1;
                state.comets.push({ pos: pos, dir: dir, vel: Math.max(0.25, v), age: 0 });
                if (state.comets.length > MAX) state.comets.shift();
            }
            state.prevHeld[note] = h;
        }

        var lk = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        function colAt(p) {
            if (!lk.length) return { r: 255, g: 255, b: 255 };
            return lk[Math.min(lk.length - 1, Math.floor(p / axisLen * lk.length))];
        }

        var speed = params.speed || 24;
        var tail  = Math.max(2, params.tail || 6);

        // Advance comets; drop those off the ends.
        var live = [];
        for (var i = 0; i < state.comets.length; i++) {
            var c = state.comets[i];
            c.age += dt;
            c.head = c.pos + c.dir * speed * c.age;
            if (c.head > -tail && c.head < axisLen + tail) live.push(c);
        }
        state.comets = live;

        // Accumulate per-cell brightness from all comet tails.
        var acc = [];
        for (var k = 0; k < axisLen; k++) acc[k] = { b: 0, src: 0 };
        for (var i = 0; i < state.comets.length; i++) {
            var c = state.comets[i];
            var head = c.head;
            for (var tOff = 0; tOff < tail; tOff++) {
                var cell = Math.round(head - c.dir * tOff);
                if (cell < 0 || cell >= axisLen) continue;
                var b = (1 - tOff / tail) * c.vel;
                if (b > acc[cell].b) { acc[cell].b = b; acc[cell].src = c.pos; }
            }
        }

        return fixtures.map(function(f, idxFix) {
            var g   = f.grid || { col: idxFix, row: 0 };
            var idx = horizontal ? g.col : g.row;   // note-axis position (cols on a wide grid)
            var a   = (idx >= 0 && idx < axisLen) ? acc[idx] : { b: 0, src: idx };
            var col = colAt(a.src);
            var out = { r: Math.round(col.r * a.b), g: Math.round(col.g * a.b), b: Math.round(col.b * a.b) };
            if (f.hasDimmer) out.dimmer = a.b;
            return out;
        });
    };

    return effect;
})()
