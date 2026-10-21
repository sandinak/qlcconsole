/*
  QLC+ Effect Script: Matrix Kaleidoscope
  A shifting pattern mirrored round the middle -- 2, 4, 6 or 8 ways -- from
  drifting clouds or from bars, turning slowly: a kaleidoscope in each box
  (or across the whole canvas).

  Pure & testable: driven by inputs._time; noise from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Kaleidoscope";
    effect.description = "A shifting pattern mirrored round the middle";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A pattern of the look's colours, mirrored round the middle like a kaleidoscope. Mirrors: 2, 4, 6 or 8-fold. Pattern: drifting clouds (noise) or bars. Scale: the size of the pattern's pieces (pixels). Speed: how fast it shifts (per beat on a speed master); Spin: turns a second the whole thing rotates (minus turns it the other way). The accent is a shimmer running through the clouds (a contour of them) or the bars' edges. Scope: a kaleidoscope in each box, or one across the whole canvas; Each box, staggered: each box a little behind the last (Stagger order; Stagger: how far, as a fraction of a shift at Speed).";

    effect.parameters = [
        { name: "folds",   description: "How many ways it is mirrored", defaultValue: 1, values: ["2-fold", "4-fold", "6-fold", "8-fold"] },
        { name: "pattern", description: "What is mirrored", defaultValue: 0, values: ["Noise", "Bars"] },
        { name: "speed",   description: "How fast it shifts (per beat on a speed master)", min: 0.02, max: 2, defaultValue: 0.3 },
        { name: "spin",    description: "Turns a second (minus = the other way)", min: -1, max: 1, defaultValue: 0.05 },
        { name: "scale",   description: "Size of the pattern's pieces (pixels)", min: 1, max: 12, defaultValue: 4 },
        { name: "scope",   description: "A kaleidoscope in each box, or one across the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    var FOLDS = [2, 4, 6, 8];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.3)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var t = num(inputs._time, 0);
            var n = FOLDS[MATRIX.clamp(Math.round(num(params.folds, 1)), 0, 3)];
            var bars = Math.round(num(params.pattern, 0)) === 1;
            var tt = t * num(params.speed, 0.3);
            var spin = t * num(params.spin, 0.05) * 2 * Math.PI;
            var scale = Math.max(0.5, num(params.scale, 4));
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 160 }, { r: 0, g: 200, b: 255 }, { r: 255, g: 200, b: 0 }]);
            var wedge = 2 * Math.PI / n;
            var zi = Math.floor(tt), zf = tt - zi;
            zf = zf * zf * (3 - 2 * zf);
            var beta = 0.7 + tt * 0.37;                                 // the bars' slant drifts

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var dx = p.x - (p.w - 1) / 2, dy = p.y - (p.h - 1) / 2;
                var r = Math.sqrt(dx * dx + dy * dy);
                var a = Math.atan2(dy, dx) + spin;
                // Fold the angle into one wedge, mirroring every other one.
                var k = Math.floor(a / wedge);
                var aa = a - k * wedge;
                if (k & 1) aa = wedge - aa;
                var qx = r * Math.cos(aa) / scale, qy = r * Math.sin(aa) / scale;
                var colour, lvl, acc;
                if (bars) {
                    var v = qx * Math.cos(beta) + qy * Math.sin(beta) - tt * 2;
                    var idx = Math.floor(v), fr = v - idx;
                    var d = Math.abs(fr - 0.5);
                    lvl = MATRIX.clamp((0.32 - d) / 0.12 + 0.5, 0, 1);
                    acc = MATRIX.clamp(1 - Math.abs(d - 0.32) / 0.08, 0, 1) * 0.6 * (lvl > 0 ? 1 : 0);
                    colour = MATRIX.pick(C, idx);
                } else {
                    var f0 = 0.65 * MATRIX.noise(qx + 1.3, qy + 0.7, zi) + 0.35 * MATRIX.noise(qx * 2.1 + 5, qy * 2.1, zi + 40);
                    var f1 = 0.65 * MATRIX.noise(qx + 1.3, qy + 0.7, zi + 1) + 0.35 * MATRIX.noise(qx * 2.1 + 5, qy * 2.1, zi + 41);
                    var w = MATRIX.clamp(((f0 * (1 - zf) + f1 * zf) - 0.5) * 1.7 + 0.5, 0, 1);
                    colour = MATRIX.ramp(C, w * 0.7 + tt * 0.1, C.length > 1);
                    lvl = MATRIX.smooth(0.1, 0.45, w);
                    // A shimmer: a contour of the clouds, wandering through them.
                    var level = 0.55 + 0.3 * Math.sin(tt * 2 * Math.PI * 0.5);
                    acc = Math.max(MATRIX.smooth(0.8, 0.95, w), 1 - Math.abs(w - level) / 0.07) * 0.6;
                }
                return MATRIX.out(f, cv, colour, lvl, acc, params);
            });
        });
    };

    return effect;
})()
