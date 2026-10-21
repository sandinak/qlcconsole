/*
  QLC+ Effect Script: Matrix Dissolve
  The canvas dissolves from one of the look's colours to the next, pixel by
  random pixel -- or to black and back -- with a glint as each one turns.

  Pure & testable: driven by the beat clock / inputs._time; each pixel's
  moment from a hash, not Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Dissolve";
    effect.description = "Pixel by random pixel, from one colour to the next";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "The canvas dissolves from one of the look's colours to the next, pixel by random pixel, or to black and back again. Timing: a dissolve on the beat (Division: how often) or free at Speed dissolves a second. Mode: colour to colour, or to and from black. Grain: the size of the pieces (pixels). Fade: how long each piece takes to turn, as a part of the dissolve (0 = it snaps). Glint: a flash of white as a piece turns (the accent). Scope: one dissolve across the whole canvas, or the same in each box; Each box, staggered: each box a little behind the last (Stagger order; Stagger: how far, as a fraction of a dissolve).";

    effect.parameters = [
        { name: "timing",   description: "A dissolve on the beat, or free at Speed", defaultValue: 1, values: MATRIX.TIMINGS },
        { name: "division", description: "On the beat: how often", defaultValue: 1, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Free: dissolves a second (per beat on a speed master)", min: 0.05, max: 4, defaultValue: 0.5 },
        { name: "mode",     description: "What it dissolves between", defaultValue: 0, values: ["Colour to colour", "To and from black"] },
        { name: "grain",    description: "Size of the pieces (pixels)", min: 1, max: 4, defaultValue: 1 },
        { name: "fade",     description: "How long a piece takes to turn (part of the dissolve)", min: 0, max: 1, defaultValue: 0.2 },
        { name: "glint",    description: "A flash of white as a piece turns", min: 0, max: 1, defaultValue: 0.3 },
        { name: "scope",    description: "One dissolve across the canvas, or the same in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.5), MATRIX.perBeat(params.division)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var onBeat = Math.round(num(params.timing, 1)) === 0;
            var clk = MATRIX.clock(inputs, num(params.speed, 0.5), onBeat, MATRIX.perBeat(params.division));
            var n = Math.floor(clk.pos), fr = clk.pos - n;
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 90 }, { r: 0, g: 140, b: 255 }, { r: 255, g: 170, b: 0 }]);
            var toBlack = Math.round(num(params.mode, 0)) === 1 || C.length < 2;
            var grain = Math.max(1, Math.round(num(params.grain, 1)));
            var fade = MATRIX.clamp(num(params.fade, 0.2), 0, 1);
            var glint = MATRIX.clamp(num(params.glint, 0.3), 0, 1);
            var perBox = MATRIX.perBox(params, 0);

            // What step @p k shows: { colour, level }.
            function shown(k) {
                if (!toBlack) return { c: MATRIX.pick(C, k), l: 1 };
                return (k & 1) ? { c: MATRIX.pick(C, (k - 1) / 2), l: 0 } : { c: MATRIX.pick(C, k / 2), l: 1 };
            }
            var was = shown(n - 1), now = shown(n);

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var gx = Math.floor(p.x / grain), gy = Math.floor(p.y / grain);
                var u = MATRIX.hash(gx + 0.31, gy * 17 + 0.57, n);
                var start = u * (1 - fade);
                var m = fade > 0 ? MATRIX.smooth(start, start + fade, fr) : (fr >= u ? 1 : 0);
                var colour, lvl;
                if (now.l === 0) { colour = was.c; lvl = 1 - m; }
                else if (was.l === 0) { colour = now.c; lvl = m; }
                else { colour = MATRIX.mix(was.c, now.c, m); lvl = 1; }
                var g = 0;
                if (glint > 0) {
                    var since = fr - (start + fade * 0.5);
                    var flash = Math.max(0.05, fade * 0.5);
                    if (since >= -flash && since < flash) g = glint * (1 - Math.abs(since) / flash);
                }
                return MATRIX.out(f, cv, colour, lvl, g, params);
            });
        });
    };

    return effect;
})()
