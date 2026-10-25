/*
  QLC+ Effect Script: Matrix Lissajous
  A bright line traces a figure-8 -- or any Lissajous figure -- across each
  box (or the whole canvas), its trail fading behind.

  Pure & testable: driven by inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Lissajous";
    effect.description = "A line traces a figure-8 with a fading trail";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A point of light traces a Lissajous figure -- a figure-8 at X 1, Y 2 -- leaving a trail that fades through the look's colours. X and Y frequency: how many times it crosses each way in one time round (1 and 1 is a circle or a line, 1 and 2 a figure-8, 3 and 2 a pretzel). Phase: turns the figure (0.25 opens a circle). Line width: in pixels. Trail: how much of the figure the trail covers. Speed: times round a second (per beat on a speed master). The accent is the head. Scope: a figure in each box, or one across the whole canvas; Each box, staggered: each box's figure a little behind the last (Stagger order; Stagger: how far, as a fraction of a time round).";

    effect.parameters = [
        { name: "speed", description: "Times round a second (per beat on a speed master)", min: 0.02, max: 2, defaultValue: 0.25 },
        { name: "xFreq", description: "Crossings left and right each time round", min: 1, max: 5, defaultValue: 1 },
        { name: "yFreq", description: "Crossings up and down each time round", min: 1, max: 5, defaultValue: 2 },
        { name: "phase", description: "Turns the figure (fraction of a cycle)", min: 0, max: 1, defaultValue: 0.25 },
        { name: "lineWidth", description: "The line's width (pixels)", min: 0.5, max: 3, defaultValue: 1 },
        { name: "trail", description: "How much of the figure the trail covers", min: 0, max: 1, defaultValue: 0.35 },
        { name: "scope", description: "A figure in each box, or one across the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.25)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var fx = Math.max(1, Math.round(num(params.xFreq, 1)));
            var fy = Math.max(1, Math.round(num(params.yFreq, 2)));
            var ph = num(params.phase, 0.25) * 2 * Math.PI;
            var lw = Math.max(0.3, num(params.lineWidth, 1));
            var trail = MATRIX.clamp(num(params.trail, 0.35), 0, 1);
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 0, g: 255, b: 200 }, { r: 120, g: 0, b: 255 }]);
            var s0 = num(inputs._time, 0) * num(params.speed, 0.25) * 2 * Math.PI;
            var paths = {};

            // The trail in a w x h area: points from the head back.
            function pathOf(w, h) {
                var key = w + "x" + h;
                if (paths[key]) return paths[key];
                var ax = (w - 1) / 2 * 0.96, ay = (h - 1) / 2 * 0.96;
                var cx = (w - 1) / 2, cy = (h - 1) / 2;
                var span = Math.max(0.02, trail) * 2 * Math.PI;
                // Enough points that no step is longer than about a pixel.
                var reach = (fx * ax + fy * ay) * span;
                var n = Math.max(6, Math.min(160, Math.ceil(reach / 0.8)));
                var pts = [];
                for (var i = 0; i <= n; i++) {
                    var s = s0 - span * i / n;
                    pts.push({ x: cx + ax * Math.sin(fx * s + ph), y: cy + ay * Math.sin(fy * s), u: i / n });
                }
                paths[key] = pts;
                return pts;
            }

            // The figure drawn into a w x h area once, segment by segment,
            // touching only the pixels near each: { level, along, accent } a pixel.
            var fields = {}, lastW = -1, lastH = -1, last = null;
            function fieldOf(w, h) {
                if (w === lastW && h === lastH) return last;   // the one the last pixel had
                var key = w + "x" + h;
                lastW = w; lastH = h;
                if (fields[key]) return (last = fields[key]);
                var pts = pathOf(w, h);
                var n = w * h, L = new Array(n), U = new Array(n), A = new Array(n), i, x, y;
                for (i = 0; i < n; i++) { L[i] = 0; U[i] = 0; A[i] = 0; }
                var half = lw / 2 + 0.5;
                for (i = 0; i + 1 < pts.length; i++) {
                    var a = pts[i], b = pts[i + 1];
                    var ax = a.x, ay = a.y, bx = b.x, by = b.y, au = a.u, du = b.u - a.u;
                    // Only a pixel nearer the segment than half lights (see c
                    // below), so no further out than that from its ends.
                    var x0 = Math.max(0, Math.floor(Math.min(ax, bx) - half)), x1 = Math.min(w - 1, Math.ceil(Math.max(ax, bx) + half));
                    var y0 = Math.max(0, Math.floor(Math.min(ay, by) - half)), y1 = Math.min(h - 1, Math.ceil(Math.max(ay, by) + half));
                    // MATRIX.segment() and MATRIX.cover(), written out.
                    var vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
                    for (y = y0; y <= y1; y++)
                        for (x = x0; x <= x1; x++) {
                            var t = 0;
                            if (l2 > 0) {
                                t = ((x - ax) * vx + (y - ay) * vy) / l2;
                                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                            }
                            var dx = x - (ax + vx * t), dy = y - (ay + vy * t);
                            var c = half - Math.sqrt(dx * dx + dy * dy);
                            if (!(c > 0)) continue;                     // covers none of it: lvl 0
                            var u = au + du * t;                        // 0 = head, 1 = end of the trail
                            var lvl = (c > 1 ? 1 : c) * (trail > 0 ? Math.pow(1 - u, 1.4) : 1);
                            var k = y * w + x;
                            if (lvl > L[k]) { L[k] = lvl; U[k] = u; }
                        }
                }
                var hx = pts[0].x, hy = pts[0].y, HR = lw * 0.6 + 1;
                for (y = Math.max(0, Math.floor(hy - HR)); y <= Math.min(h - 1, Math.ceil(hy + HR)); y++)
                    for (x = Math.max(0, Math.floor(hx - HR)); x <= Math.min(w - 1, Math.ceil(hx + HR)); x++)
                        A[y * w + x] = MATRIX.cover(Math.sqrt((x - hx) * (x - hx) + (y - hy) * (y - hy)), lw * 1.2) * 0.9;
                fields[key] = last = { L: L, U: U, A: A, K: new Array(n) };
                return last;
            }

            // A head off its area's grid (past the columns it claims, or
            // between cells) worked out on its own, as fieldOf() would:
            // [level, along, accent].
            function alone(w, h, px, py) {
                var pts = pathOf(w, h), half = lw / 2 + 0.5, best = 0, along = 0;
                for (var i = 0; i + 1 < pts.length; i++) {
                    var seg = MATRIX.segment(px, py, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
                    var c = half - seg.d;
                    if (!(c > 0)) continue;
                    var u = pts[i].u + (pts[i + 1].u - pts[i].u) * seg.t;
                    var lvl = (c > 1 ? 1 : c) * (trail > 0 ? Math.pow(1 - u, 1.4) : 1);
                    if (lvl > best) { best = lvl; along = u; }
                }
                var hx = pts[0].x, hy = pts[0].y;
                var acc = MATRIX.cover(Math.sqrt((px - hx) * (px - hx) + (py - hy) * (py - hy)), lw * 1.2) * 0.9;
                return [best, along, acc];
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                if (!(p.x >= 0 && p.x < p.w && p.y >= 0 && p.y < p.h
                      && Math.floor(p.x) === p.x && Math.floor(p.y) === p.y)) {
                    var o = alone(p.w, p.h, p.x, p.y);
                    return MATRIX.out(f, cv, MATRIX.ramp(C, Math.min(1, o[1] * 2), false), o[0], o[2], params);
                }
                var fl = fieldOf(p.w, p.h), k = p.y * p.w + p.x;
                // Along the trail through the look's colours: the head the
                // first (once a pixel of the area: every box's is the same).
                var c = fl.K[k];
                if (c === undefined) c = fl.K[k] = MATRIX.ramp(C, Math.min(1, fl.U[k] * 2), false);
                return MATRIX.out(f, cv, c, fl.L[k], fl.A[k], params);
            });
        });
    };

    return effect;
})()
