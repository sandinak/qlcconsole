/*
  QLC+ Effect Script: Matrix Checker
  A checkerboard that swaps on the beat: two colours trade squares, or the
  lit squares and the dark ones do, with a white flash on the change.

  Pure & testable: driven by the beat clock / inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Checker";
    effect.description = "A checkerboard that swaps on the beat";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A checkerboard of the look's first two colours (or the first and dark) that swaps squares. Swap: colours trade places, the lit squares and the dark ones trade, or a smooth crossfade. Timing: on the beat (Division: how often) or free at Speed swaps a second. Square: its size in pixels. Fade: how much of each step the change takes. Scope: squares counted from each box's corner, or across the whole canvas. Flash: a white flash on each change (the accent).";

    effect.parameters = [
        { name: "timing",   description: "Swap on the beat, or free at Speed", defaultValue: 0, values: MATRIX.TIMINGS },
        { name: "division", description: "On the beat: how often it swaps", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Free: swaps a second (per beat on a speed master)", min: 0.1, max: 12, defaultValue: 2 },
        { name: "swap",     description: "What swaps", defaultValue: 0, values: ["Colours", "On and off", "Crossfade"] },
        { name: "square",   description: "Square size (pixels)", min: 1, max: 8, defaultValue: 2 },
        { name: "fade",     description: "How much of each step the change takes", min: 0, max: 1, defaultValue: 0.1 },
        { name: "flash",    description: "White flash on each change", min: 0, max: 1, defaultValue: 0.5 },
        { name: "scope",    description: "Squares from each box's corner, or across the canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var onBeat = Math.round(num(params.timing, 0)) === 0;
        var clk = MATRIX.clock(inputs, num(params.speed, 2), onBeat, MATRIX.perBeat(params.division));
        var step = Math.floor(clk.pos), fr = clk.pos - step;
        var swap = Math.round(num(params.swap, 0));
        // Drawn only in white (a box in its white mode, or White only): two
        // colours would be one, so the squares go on and off instead.
        if (swap === 0 && (!cv.anyRGB || Math.round(num(params.white, 0)) === 4))
            swap = 1;
        var sq = Math.max(1, Math.round(num(params.square, 1)));
        var fade = MATRIX.clamp(num(params.fade, 0.1), 0, 1);
        var flash = MATRIX.clamp(num(params.flash, 0.5), 0, 1);
        var perBox = Math.round(num(params.scope, 1)) === 1;
        var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 80 }, { r: 0, g: 60, b: 255 }]);
        var a = C[0], b = C.length > 1 ? C[1] : null;
        var black = { r: 0, g: 0, b: 0 };

        // How far into the change: 0 = just swapped, 1 = settled.
        var into = fade > 0 ? MATRIX.smooth(0, fade, fr) : 1;
        var hit = flash * Math.max(0, 1 - fr / 0.2);

        function cellAt(parity, k) {
            // { colour, level } of a square of @p parity at step @p k.
            var on = ((parity + k) & 1) === 0;
            if (swap === 1 || !b)
                return on ? { c: a, l: 1 } : { c: a, l: 0 };
            return { c: on ? a : b, l: 1 };
        }

        return fixtures.map(function(f) {
            var p = MATRIX.point(f, cv, perBox);
            var parity = (Math.floor(p.x / sq) + Math.floor(p.y / sq)) & 1;
            var colour, lvl, acc = 0;
            if (swap === 2) {
                var w = 0.5 - 0.5 * Math.cos(Math.PI * 2 * (clk.pos / 2));
                var x = parity ? w : 1 - w;
                if (b) { colour = MATRIX.mix(a, b, x); lvl = 1; }
                else { colour = a; lvl = 1 - x; }
            } else {
                var now = cellAt(parity, step), was = cellAt(parity, step - 1);
                var ca = MATRIX.mix(was.c === null ? black : was.c, now.c, into);
                colour = ca;
                lvl = was.l + (now.l - was.l) * into;
                if (now.l > 0 && (now.c !== was.c || now.l !== was.l)) acc = hit;
            }
            return MATRIX.out(f, cv, colour, lvl, acc, params);
        });
    };

    return effect;
})()
