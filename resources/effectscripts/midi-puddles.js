/*
  QLC+ Effect Script: MIDI Puddles
  Each struck note drops a ripple that radiates outward from that note's position
  across the pixel grid, over a held-note glow base — like drops in a pond. Port
  of the WLED "MIDI Puddles" idea, using the grid context for ripple propagation.

  Subscribes to the host "midi" data channel. Colour from the look's palette.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "MIDI Puddles";
    effect.description = "Ripples radiate from each played note across the grid";
    effect.author      = "WLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["midi"];
    effect.notes = "A soft held-note glow plus a ripple that radiates from each note the instant it's struck (mapped across the fixture grid, or a 1-D strip). Speed sets ripple travel; Width the ring thickness. Notes map across [Note low..Note high]. Colours from the look's palette.";

    effect.parameters = [
        { name: "noteLow",  description: "Lowest MIDI note (C2=36, C3=48, C4=60)",  min: 0, max: 127, defaultValue: 36 },
        { name: "noteHigh", description: "Highest MIDI note (C6=84, C7=96)",        min: 0, max: 127, defaultValue: 84 },
        { name: "axis",     description: "Note spread axis", defaultValue: 0, values: ["Horizontal (columns)", "Vertical (rows)", "Auto (wider)"] },
        { name: "speed",    description: "Ripple speed (cells/sec)", min: 1, max: 60, defaultValue: 14 },
        { name: "width",    description: "Ripple width (cells)",     min: 1, max: 12, defaultValue: 3 },
        { name: "glow",     description: "Held-note glow fade (s)",  min: 0.1, max: 4, defaultValue: 0.7 }
    ];

    var MAX = 24;   // ripple pool

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var g0    = fixtures[0].grid || { cols: n, rows: 1 };
        var cols  = g0.cols || n, rows = g0.rows || 1;
        // Notes spread along the chosen grid axis; the ripple travels along it and
        // the other axis mirrors as bars. Default Horizontal (columns).
        var ax = params.axis | 0;   // 0=Horizontal, 1=Vertical, 2=Auto
        var horizontal = (ax === 1) ? false : (ax === 2) ? (cols >= rows) : true;
        var axisLen = Math.max(1, horizontal ? cols : rows);

        var m  = data && data.midi;
        var lo = params.noteLow | 0, hi = params.noteHigh | 0;
        if (hi <= lo) hi = lo + 1;

        if (!state.ripples) {
            state.ripples = [];
            state.prevHeld = [];
            state.lvl = [];
            for (var k = 0; k < 128; k++) { state.prevHeld[k] = false; state.lvl[k] = 0; }
            state.t = inputs._time || 0;
        }
        var t  = inputs._time || 0;
        var dt = Math.max(0, t - state.t);
        state.t = t;

        var vel = m && m.velocity, held = m && m.held;
        var glowRate = 1 / Math.max(0.05, params.glow || 0.7);

        // Note bookkeeping: decay glow + spawn a ripple on each fresh strike.
        for (var note = lo; note <= hi; note++) {
            var h = held ? held[note] : false;
            var v = vel  ? vel[note] / 127 : 0;
            if (h && !state.prevHeld[note]) {   // fresh strike
                var pos = lo === hi ? 0 : Math.round((note - lo) / (hi - lo) * (axisLen - 1));
                state.ripples.push({ pos: pos, age: 0, vel: Math.max(0.2, v) });
                if (state.ripples.length > MAX) state.ripples.shift();
            }
            if (h) { if (v > state.lvl[note]) state.lvl[note] = v; }
            else   { state.lvl[note] = Math.max(0, state.lvl[note] - glowRate * dt); }
            state.prevHeld[note] = h;
        }

        var lk    = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        function colAt(p) {
            if (!lk.length) return { r: 255, g: 255, b: 255 };
            return lk[Math.min(lk.length - 1, Math.floor(p / axisLen * lk.length))];
        }

        var speed = params.speed || 14;
        var width = Math.max(1, params.width || 3);

        // Advance ripples; drop the dead ones.
        var live = [];
        for (var i = 0; i < state.ripples.length; i++) {
            var r = state.ripples[i];
            r.age += dt;
            if (r.age * speed < axisLen + width) live.push(r);
        }
        state.ripples = live;

        return fixtures.map(function(f, idxFix) {
            var g   = f.grid || { col: idxFix, row: 0 };
            var idx = horizontal ? g.col : g.row;   // note-axis position (cols on a wide grid)

            // Held-note glow base (this cell's note band along the note axis).
            var a = lo + Math.floor(idx       / axisLen * (hi - lo + 1));
            var b = lo + Math.floor((idx + 1) / axisLen * (hi - lo + 1));
            if (b <= a) b = a + 1;
            var base = 0;
            for (var note = a; note < b && note <= hi; note++)
                if (state.lvl[note] > base) base = state.lvl[note];

            // Ripple contributions (a ring at radius = age*speed from origin).
            var rip = 0, ripPos = idx;
            for (var i = 0; i < state.ripples.length; i++) {
                var r = state.ripples[i];
                var radius = r.age * speed;
                var d = Math.abs(idx - r.pos);
                var edge = Math.abs(d - radius);
                if (edge < width) {
                    var amp = (1 - edge / width) * Math.max(0, 1 - r.age * speed / axisLen) * r.vel;
                    if (amp > rip) { rip = amp; ripPos = r.pos; }
                }
            }

            var b2  = Math.min(1, base * 0.6 + rip);
            var col = colAt(rip > base * 0.6 ? ripPos : idx);
            var out = { r: Math.round(col.r * b2), g: Math.round(col.g * b2), b: Math.round(col.b * b2) };
            if (f.hasDimmer) out.dimmer = b2;
            return out;
        });
    };

    return effect;
})()
