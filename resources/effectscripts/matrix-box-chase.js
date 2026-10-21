/*
  QLC+ Effect Script: Matrix Box Chase
  Whole boxes light in turn round the rig -- left to right, bouncing, out
  from the middle, odd and even, at random -- stepping on the beat, with a
  white hit as each comes on. On a single box, its fifths take turns.

  Pure & testable: driven by the beat clock / inputs._time; random order
  from a hash, not Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Box Chase";
    effect.description = "Whole boxes light in turn round the rig, on the beat";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    // The host's spread: each box's place in the chase comes from its phase.
    effect.hostSpread  = "order";
    effect.notes = "Each box (each fixture: a box, a bar, a par) lights as a whole, in turn, left to right as they hang. Order: left to right, right to left, bounce, out from the middle, in to the middle, odd then even, or random. Lit: how many are on at once. Timing: a step on the beat (Division: how often) or free at Speed steps a second. Fade: how long a box takes to go out after its turn. Colours: the look's colours, one a step. Flash: a white hit as a box comes on (the accent). With one box, its five vertical slices take the turns. With the look's Spread on, the spread decides the order instead.";

    effect.parameters = [
        { name: "timing",   description: "Step on the beat, or free at Speed", defaultValue: 0, values: MATRIX.TIMINGS },
        { name: "division", description: "On the beat: how often it steps", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Free: steps a second (per beat on a speed master)", min: 0.2, max: 16, defaultValue: 4 },
        { name: "order",    description: "Which box goes next", defaultValue: 0,
          values: ["Left to right", "Right to left", "Bounce", "Out from the middle", "In to the middle", "Odd then even", "Random"] },
        { name: "lit",      description: "Boxes on at once", min: 1, max: 8, defaultValue: 1 },
        { name: "fade",     description: "Fade out after its turn (steps)", min: 0, max: 4, defaultValue: 0.5 },
        { name: "flash",    description: "White hit as a box comes on", min: 0, max: 1, defaultValue: 0.6 }
    ].concat(MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var onBeat = Math.round(num(params.timing, 0)) === 0;
        var clk = MATRIX.clock(inputs, num(params.speed, 4), onBeat, MATRIX.perBeat(params.division));
        var order = Math.round(num(params.order, 0));
        var lit = Math.max(1, Math.round(num(params.lit, 1)));
        var fade = Math.max(0, num(params.fade, 0.5));
        var flash = MATRIX.clamp(num(params.flash, 0.6), 0, 1);
        var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 0 }, { r: 0, g: 0, b: 255 }]);
        var host = !!inputs._spread;
        var single = cv.count < 2;
        var units = single ? 5 : cv.count;
        var step = Math.floor(clk.pos), fr = clk.pos - step;

        // The steps of one round, as unit lists: which units each step lights.
        function stepsOf(n) {
            var s = [], i;
            if (order === 1) for (i = n - 1; i >= 0; i--) s.push([i]);
            else if (order === 2) {
                for (i = 0; i < n; i++) s.push([i]);
                for (i = n - 2; i > 0; i--) s.push([i]);
            } else if (order === 3 || order === 4) {
                var mid = (n - 1) / 2, rings = [];
                for (i = 0; i < n; i++) {
                    var r = Math.floor(Math.abs(i - mid));
                    (rings[r] = rings[r] || []).push(i);
                }
                if (order === 4) rings.reverse();
                for (i = 0; i < rings.length; i++) if (rings[i]) s.push(rings[i]);
            } else if (order === 5) {
                var odd = [], even = [];
                for (i = 0; i < n; i++) (i & 1 ? odd : even).push(i);
                s.push(even); s.push(odd);
            } else for (i = 0; i < n; i++) s.push([i]);
            return s;
        }
        var steps = stepsOf(units);
        var len = steps.length;

        // When did unit @p u last start its turn, in steps ago (0 = now)?
        function since(u, k) {
            if (order === 6) {
                for (var a = 0; a < len + lit + 8; a++) {
                    var want = Math.floor(MATRIX.hash(k - a, 31) * units);
                    for (var b = 0; b < lit; b++)
                        if ((want + b) % units === u) return a;
                }
                return 1e9;
            }
            for (var back = 0; back < len; back++) {
                var at = ((k - back) % len + len) % len;
                for (var j = 0; j < lit; j++) {
                    var g = steps[(at - j + len * 8) % len];
                    if (g.indexOf(u) >= 0) return back;
                }
            }
            return 1e9;
        }

        // Each unit's level and accent, once a tick.
        var lv = [], ac = [], col = [];
        for (var u = 0; u < units; u++) {
            var ago;
            if (host) {
                // Its place in the round from the host's spread.
                var ph = 0;
                for (var q = 0; q < fixtures.length; q++) {
                    var fq = fixtures[q], bq = cv.box[fq.id];
                    if (bq && bq.index === u) { ph = num(fq.phase, 0); break; }
                }
                var mine = Math.floor(((ph % 1) + 1) % 1 * units);
                var rel = ((step - mine) % units + units) % units;
                ago = rel < lit ? 0 : rel - lit + 1;
            } else ago = since(u, step);
            var on = ago === 0 ? 1 : (fade > 0 ? Math.max(0, 1 - (ago - 1 + fr) / fade) : 0);
            lv.push(on);
            ac.push(ago === 0 ? flash * Math.max(0, 1 - fr / 0.25) : 0);
            col.push(MATRIX.pick(C, step - (ago === 0 ? 0 : ago)));
        }

        return fixtures.map(function(f) {
            var b = cv.box[f.id];
            var u;
            if (single) {
                var g = f.grid || { col: 0 };
                var w = b ? b.w : cv.cols;
                u = Math.min(units - 1, Math.floor((g.col - (b ? b.c0 : 0)) / Math.max(1, w) * units));
            } else u = b ? b.index : 0;
            return MATRIX.out(f, cv, col[u], lv[u], ac[u], params);
        });
    };

    return effect;
})()
