/*
  QLC+ Effect Script: Matrix Starfield
  Stars stream out from a point, faster and brighter as they come -- the
  jump to warp speed, across the rig or in each box.

  Pure & testable: driven by inputs._time; the stars from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Starfield";
    effect.description = "Stars stream out from the middle: warp speed";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Stars fly out from a point, speeding up and brightening as they come, each in one of the look's colours (white without). Density: how many stars. Speed: how fast they fly (per beat on a speed master). Trails: how long a streak each one leaves -- none for points of light, long for warp. Centre: where they come from -- the middle, an edge, or wandering about. The accent is each star's head as it gets close. Scope: one starfield across the whole canvas, or one in each box; Each box, staggered: each box's stars a moment behind the last (Stagger order; Stagger: how far, as a fraction of a star's flight).";

    effect.parameters = [
        { name: "speed",   description: "How fast they fly (per beat on a speed master)", min: 0.1, max: 4, defaultValue: 0.8 },
        { name: "density", description: "How many stars", min: 0.05, max: 1, defaultValue: 0.45 },
        { name: "trails",  description: "How long a streak each leaves", min: 0, max: 1, defaultValue: 0.5 },
        { name: "centre",  description: "Where the stars come from", defaultValue: 0,
          values: ["Middle", "Left edge", "Right edge", "Top", "Bottom", "Wandering"] },
        { name: "scope",   description: "One starfield across the canvas, or one in each box", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.8)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var t = num(inputs._time, 0);
            var tt = t * num(params.speed, 0.8);
            var density = MATRIX.clamp(num(params.density, 0.45), 0, 1);
            var trails = MATRIX.clamp(num(params.trails, 0.5), 0, 1);
            var centre = Math.round(num(params.centre, 0));
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 255, b: 255 }, { r: 170, g: 200, b: 255 }]);
            var fields = {};

            // The stars of a w x h area, as segments in its pixels (tail to head).
            function starsOf(w, h) {
                var key = w + "x" + h;
                if (fields[key]) return fields[key];
                var cx = (w - 1) / 2, cy = (h - 1) / 2, a0 = 0, a1 = 1;
                if (centre === 1) { cx = -0.5; a0 = -0.25; a1 = 0.25; }
                else if (centre === 2) { cx = w - 0.5; a0 = 0.25; a1 = 0.75; }
                else if (centre === 3) { cy = -0.5; a0 = 0; a1 = 0.5; }
                else if (centre === 4) { cy = h - 0.5; a0 = 0.5; a1 = 1; }
                else if (centre === 5) {
                    cx = (0.5 + 0.3 * Math.sin(t * 0.41)) * (w - 1);
                    cy = (0.5 + 0.3 * Math.sin(t * 0.67 + 1)) * (h - 1);
                }
                // How far a star goes, each way: past the far edge.
                var ex = Math.max(cx + 1, w - cx) + 0.5, ey = Math.max(cy + 1, h - cy) + 0.5;
                var n = Math.max(3, Math.round(26 * density * Math.sqrt(w * h / 75)));
                var list = [];
                for (var i = 0; i < n; i++) {
                    var pos = tt * (0.75 + 0.5 * MATRIX.hash(i, 3)) + MATRIX.hash(i, 1);
                    var k = Math.floor(pos), age = pos - k;
                    var th = 2 * Math.PI * (a0 + (a1 - a0) * MATRIX.hash(i, k, 2));
                    var dx = Math.cos(th) * ex, dy = Math.sin(th) * ey;
                    // Perspective: slow at first, then rushing out.
                    var r = Math.pow(age, 2.2), r0 = Math.pow(Math.max(0, age - 0.08 - trails * 0.5), 2.2);
                    var s = { hx: cx + dx * r, hy: cy + dy * r, tx: cx + dx * r0, ty: cy + dy * r0,
                              b: Math.min(1, age * 2.2), w: 0.6 + 0.9 * age, age: age,
                              c: MATRIX.pick(C, i + k) };
                    s.x0 = Math.min(s.hx, s.tx) - s.w - 1; s.x1 = Math.max(s.hx, s.tx) + s.w + 1;
                    s.y0 = Math.min(s.hy, s.ty) - s.w - 1; s.y1 = Math.max(s.hy, s.ty) + s.w + 1;
                    list.push(s);
                }
                fields[key] = list;
                return list;
            }

            // The stars drawn into a w x h area once, each touching only the
            // pixels near it: { level, colour, accent } a pixel.
            var drawn = {};
            function fieldOf(w, h) {
                var key = w + "x" + h;
                if (drawn[key]) return drawn[key];
                var stars = starsOf(w, h);
                var n = w * h, L = new Array(n), K = new Array(n), A = new Array(n), i, x, y;
                for (i = 0; i < n; i++) { L[i] = 0; K[i] = null; A[i] = 0; }
                for (i = 0; i < stars.length; i++) {
                    var s = stars[i];
                    var x0 = Math.max(0, Math.floor(s.x0)), x1 = Math.min(w - 1, Math.ceil(s.x1));
                    var y0 = Math.max(0, Math.floor(s.y0)), y1 = Math.min(h - 1, Math.ceil(s.y1));
                    var glow = MATRIX.smooth(0.45, 0.95, s.age) * 0.8;
                    for (y = y0; y <= y1; y++)
                        for (x = x0; x <= x1; x++) {
                            var seg = MATRIX.segment(x, y, s.tx, s.ty, s.hx, s.hy);
                            var lvl = MATRIX.cover(seg.d, s.w) * s.b * (0.2 + 0.8 * seg.t);
                            var k = y * w + x;
                            if (lvl > L[k]) { L[k] = lvl; K[k] = s.c; }
                            if (glow > 0) {
                                var dh = Math.sqrt((x - s.hx) * (x - s.hx) + (y - s.hy) * (y - s.hy));
                                A[k] = Math.max(A[k], MATRIX.cover(dh, s.w * 0.8) * glow);
                            }
                        }
                }
                drawn[key] = { L: L, K: K, A: A };
                return drawn[key];
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var fl = fieldOf(p.w, p.h), k = p.y * p.w + p.x;
                return MATRIX.out(f, cv, fl.K[k] || C[0], fl.L[k], fl.A[k], params);
            });
        });
    };

    return effect;
})()
