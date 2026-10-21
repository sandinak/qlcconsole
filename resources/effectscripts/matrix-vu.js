/*
  QLC+ Effect Script: Matrix Box VU
  Every box is a VU meter: it fills with the level of the live audio (or
  one band of it, or a band a box from low to high), a peak dot holding
  the top. Without audio it plays a convincing one of its own, on the
  beat.

  Pure & testable: driven by inputs/data; the simulation from a hash, not
  Math.random. The meters' fall and peaks are kept in state.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Box VU";
    effect.description = "Each box a VU meter: live audio, or simulated on the beat";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.dataChannels = ["audio"];
    effect.notes = "Each box is one VU meter, filling with the music. Source: the live audio input (Inputs/Outputs) -- simulated on the beat while there is none -- or always simulated. Band: the whole level in every box, low to high across the boxes (the bass on the left, like a graphic equaliser a box a band), just the bass, or just the highs. Gain: how high the meters reach; Release: how quickly they fall. Peaks: dots that hold the top and fall slowly (seconds; 0 = none; the accent). From: the bottom, the middle up and down, the left, or the middle out left and right. Colours: green to red up the meter, the look's colours up the meter, or one of the look's colours a box. Scope: a meter in each box, or one across the whole canvas; Each box, staggered: each box shows the music a little after the last, so the level rolls across the rig (Stagger order; Stagger: how far, as a fraction of a beat).";

    effect.parameters = [
        { name: "source",  description: "What drives the meters", defaultValue: 0, values: ["Audio input (simulated without)", "Simulated"] },
        { name: "band",    description: "What each meter shows", defaultValue: 0,
          values: ["Whole level", "Low to high across the boxes", "Bass", "Highs"] },
        { name: "gain",    description: "How high the meters reach", min: 0.2, max: 3, defaultValue: 1 },
        { name: "release", description: "How quickly the meters fall (seconds)", min: 0.02, max: 1.5, defaultValue: 0.25 },
        { name: "peaks",   description: "Peak dots: how long they hold (seconds; 0 = none)", min: 0, max: 2, defaultValue: 0.6 },
        { name: "from",    description: "Where the meter fills from", defaultValue: 0,
          values: ["Bottom", "Middle, up and down", "Left", "Middle, left and right"] },
        { name: "colours", description: "Colouring", defaultValue: 0,
          values: ["Green to red", "Look colours up the meter", "A look colour each box"] },
        { name: "scope",   description: "A meter in each box, or one across the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(1));

    var GYR = [{ r: 0, g: 255, b: 0 }, { r: 255, g: 220, b: 0 }, { r: 255, g: 0, b: 0 }];

    // The level of one meter, from the audio's bands, for @p u 0..1 of the way
    // across the boxes.
    function fromAudio(a, mode, u) {
        var bands = a.bands || [];
        var n = bands.length;
        if (n === 0 || mode === 0) return MATRIX.num(a.level, 0) > 0 ? Math.min(1, MATRIX.num(a.level, 0) * 1.6) : MATRIX.num(a.peak, 0);
        var lo, hi;
        if (mode === 1) { lo = Math.floor(u * n * 0.999); hi = lo + 1; }
        else if (mode === 2) { lo = 0; hi = Math.max(1, Math.round(n / 6)); }
        else { lo = Math.round(n / 2); hi = n; }
        var s = 0, c = 0;
        for (var i = lo; i < hi && i < n; i++) { s = Math.max(s, MATRIX.num(bands[i], 0)); c++; }
        return c ? s : 0;
    }

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var live = (Math.round(num(params.source, 0)) === 0 && data && data.audio && data.audio.bands)
                   ? data.audio : null;
        if (live && MATRIX.scope(params, 1) === 2)
            MATRIX.record(state, "__audio", num(inputs._time, 0), live, 8);
        var bpm = num(inputs._bpm, 0);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, bpm > 0 ? bpm / 60 : 2, 1),
                             function(fixtures, inputs, state, root) {
            var t = num(inputs._time, 0);
            var mode = Math.round(num(params.band, 0));
            var gain = Math.max(0.05, num(params.gain, 1));
            var release = Math.max(0.01, num(params.release, 0.25));
            var hold = Math.max(0, num(params.peaks, 0.6));
            var from = Math.round(num(params.from, 0));
            var scheme = Math.round(num(params.colours, 0));
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, GYR);
            var audio = live;
            if (audio && root !== state) audio = MATRIX.recalled(root, "__audio", t) || live;
            var beat = bpm > 0 ? num(inputs._beat, 0) : (t * 2) - Math.floor(t * 2);
            var beatNo = bpm > 0 ? Math.floor(num(inputs._beatCount, 0)) : Math.floor(t * 2);
            var units = perBox ? Math.max(1, cv.count) : 1;

            // Each meter's level now: fast up, falling at Release.
            if (state.v === undefined || state.v.length !== units) { state.v = []; state.pk = []; state.age = []; }
            var dt = state.lastT === undefined ? 0 : Math.max(0, Math.min(0.5, t - state.lastT));
            state.lastT = t;
            var lv = [];
            for (var u = 0; u < units; u++) {
                var x = units > 1 ? u / (units - 1) : 0.5;
                var target;
                if (audio) target = fromAudio(audio, mode, x);
                else {
                    // The simulation: a kick on every beat, louder in the bass,
                    // and a wobble of its own in each meter.
                    var lean = mode === 1 ? 1 - 0.6 * x : (mode === 3 ? 0.5 : 1);
                    var kick = Math.pow(1 - beat, 2.5) * (0.65 + 0.35 * MATRIX.hash(u, beatNo, 3)) * lean;
                    var wob = MATRIX.noise(t * (2 + x * 3), u * 4.1, 17);
                    target = 0.12 + 0.5 * kick + 0.38 * wob * (mode === 2 ? 0.6 : 1);
                }
                target = MATRIX.clamp(target * gain, 0, 1);
                var v = state.v[u] === undefined ? target : state.v[u];
                v = target >= v ? target : Math.max(target, v - dt / release);
                state.v[u] = v;
                var pk = state.pk[u] === undefined ? v : state.pk[u];
                if (v >= pk) { pk = v; state.age[u] = 0; }
                else {
                    state.age[u] = (state.age[u] || 0) + dt;
                    if (state.age[u] > hold) pk = Math.max(v, pk - dt);
                }
                state.pk[u] = pk;
                lv.push(v);
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var u = (perBox && p.box) ? Math.min(units - 1, p.box.index) : 0;
                var level = lv[u], peak = state.pk[u];
                // How far up the meter this pixel is (0..1) and one pixel's step.
                var hgt, step;
                if (from === 1 || from === 3) {
                    var vert = from === 1;
                    var len = vert ? p.h : p.w, c = (len - 1) / 2, at = vert ? p.y : p.x;
                    hgt = len > 1 ? Math.abs(at - c) / Math.max(0.5, c + 0.5) : 0;
                    step = 1 / Math.max(1, Math.ceil(len / 2));
                } else if (from === 2) {
                    hgt = p.w > 1 ? p.x / p.w : 0;
                    step = 1 / Math.max(1, p.w);
                } else {
                    hgt = p.h > 1 ? (p.h - 1 - p.y) / p.h : 0;
                    step = 1 / Math.max(1, p.h);
                }
                // The top pixel lit in part, so the meter rises smoothly.
                var on = level > hgt ? Math.min(1, (level - hgt) / step) : 0;
                var acc = 0;
                if (hold > 0 && peak > step && peak - hgt > 0 && peak - hgt <= step && on < 1) acc = 1;
                var colour;
                if (scheme === 1) colour = MATRIX.ramp(C, hgt, false);
                else if (scheme === 2) colour = MATRIX.pick(C, u);
                else colour = MATRIX.ramp(GYR, hgt, false);
                return MATRIX.out(f, cv, colour, on, acc, params);
            });
        });
    };

    return effect;
})()
