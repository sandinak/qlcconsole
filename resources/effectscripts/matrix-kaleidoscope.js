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
    // Each area's pixels' distance and angle from its middle ("15x5": {r, a}).
    var GEOMETRY = {};

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

            var level = 0.55 + 0.3 * Math.sin(tt * 2 * Math.PI * 0.5);   // the shimmer's contour
            var sin = Math.sin, cos = Math.cos, floor = Math.floor, nC = C.length;
            var LVL_SPAN = 0.45 - 0.1, ACC_SPAN = 0.95 - 0.8;
            // MATRIX.hash()'s seed terms for the four noise fields.
            var c0 = (zi || 0) * 37.719, c1 = ((zi + 1) || 0) * 37.719;
            var c40 = ((zi + 40) || 0) * 37.719, c41 = ((zi + 41) || 0) * 37.719;

            // One pixel, @p r from the middle at angle @p a0 (before the
            // spin): [colour, level, accent].
            function pixel(r, a0) {
                var a = a0 + spin;
                // Fold the angle into one wedge, mirroring every other one.
                var k = floor(a / wedge);
                var aa = a - k * wedge;
                if (k & 1) aa = wedge - aa;
                var qx = r * cos(aa) / scale, qy = r * sin(aa) / scale;
                var colour, lvl, acc;
                if (bars) {
                    var v = qx * Math.cos(beta) + qy * Math.sin(beta) - tt * 2;
                    var idx = Math.floor(v), fr = v - idx;
                    var d = Math.abs(fr - 0.5);
                    lvl = MATRIX.clamp((0.32 - d) / 0.12 + 0.5, 0, 1);
                    acc = MATRIX.clamp(1 - Math.abs(d - 0.32) / 0.08, 0, 1) * 0.6 * (lvl > 0 ? 1 : 0);
                    colour = MATRIX.pick(C, idx);
                } else {
                    // MATRIX.noise() written out: two fields, each at this
                    // tick's two seeds -- the same corners, other hashes.
                    var x, y, xi, yi, xf, yf, u, v1, ax, ay, bx, cy, e, ha, hb, hc, hd, n0, n1, n40, n41;
                    x = qx + 1.3; y = qy + 0.7;
                    xi = floor(x); yi = floor(y); xf = x - xi; yf = y - yi;
                    u = xf * xf * (3 - 2 * xf); v1 = yf * yf * (3 - 2 * yf);
                    ax = xi * 12.9898; bx = (xi + 1) * 12.9898;
                    ay = (yi || 0) * 78.233; cy = ((yi + 1) || 0) * 78.233;
                    e = sin(ax + ay + c0 + 0.5) * 43758.5453; ha = e - floor(e);
                    e = sin(bx + ay + c0 + 0.5) * 43758.5453; hb = e - floor(e);
                    e = sin(ax + cy + c0 + 0.5) * 43758.5453; hc = e - floor(e);
                    e = sin(bx + cy + c0 + 0.5) * 43758.5453; hd = e - floor(e);
                    n0 = ha + (hb - ha) * u + (hc - ha) * v1 + (ha - hb - hc + hd) * u * v1;
                    e = sin(ax + ay + c1 + 0.5) * 43758.5453; ha = e - floor(e);
                    e = sin(bx + ay + c1 + 0.5) * 43758.5453; hb = e - floor(e);
                    e = sin(ax + cy + c1 + 0.5) * 43758.5453; hc = e - floor(e);
                    e = sin(bx + cy + c1 + 0.5) * 43758.5453; hd = e - floor(e);
                    n1 = ha + (hb - ha) * u + (hc - ha) * v1 + (ha - hb - hc + hd) * u * v1;
                    x = qx * 2.1 + 5; y = qy * 2.1;
                    xi = floor(x); yi = floor(y); xf = x - xi; yf = y - yi;
                    u = xf * xf * (3 - 2 * xf); v1 = yf * yf * (3 - 2 * yf);
                    ax = xi * 12.9898; bx = (xi + 1) * 12.9898;
                    ay = (yi || 0) * 78.233; cy = ((yi + 1) || 0) * 78.233;
                    e = sin(ax + ay + c40 + 0.5) * 43758.5453; ha = e - floor(e);
                    e = sin(bx + ay + c40 + 0.5) * 43758.5453; hb = e - floor(e);
                    e = sin(ax + cy + c40 + 0.5) * 43758.5453; hc = e - floor(e);
                    e = sin(bx + cy + c40 + 0.5) * 43758.5453; hd = e - floor(e);
                    n40 = ha + (hb - ha) * u + (hc - ha) * v1 + (ha - hb - hc + hd) * u * v1;
                    e = sin(ax + ay + c41 + 0.5) * 43758.5453; ha = e - floor(e);
                    e = sin(bx + ay + c41 + 0.5) * 43758.5453; hb = e - floor(e);
                    e = sin(ax + cy + c41 + 0.5) * 43758.5453; hc = e - floor(e);
                    e = sin(bx + cy + c41 + 0.5) * 43758.5453; hd = e - floor(e);
                    n41 = ha + (hb - ha) * u + (hc - ha) * v1 + (ha - hb - hc + hd) * u * v1;
                    var f0 = 0.65 * n0 + 0.35 * n40;
                    var f1 = 0.65 * n1 + 0.35 * n41;
                    var w = ((f0 * (1 - zf) + f1 * zf) - 0.5) * 1.7 + 0.5;
                    w = w < 0 ? 0 : (w > 1 ? 1 : w);
                    // MATRIX.ramp(C, .., true) and MATRIX.smooth(), written out.
                    if (nC === 1) {
                        colour = C[0];
                    } else {
                        var uu = w * 0.7 + tt * 0.1;
                        var xr = (uu - floor(uu)) * nC, ir = floor(xr), tr = xr - ir;
                        var ca = C[ir % nC], cb = C[(ir + 1) % nC];
                        tr = tr < 0 ? 0 : (tr > 1 ? 1 : tr);
                        colour = { r: ca.r + (cb.r - ca.r) * tr, g: ca.g + (cb.g - ca.g) * tr, b: ca.b + (cb.b - ca.b) * tr };
                    }
                    var sl = (w - 0.1) / LVL_SPAN, sa = (w - 0.8) / ACC_SPAN;
                    sl = sl < 0 ? 0 : (sl > 1 ? 1 : sl);
                    sa = sa < 0 ? 0 : (sa > 1 ? 1 : sa);
                    lvl = sl * sl * (3 - 2 * sl);                       // smooth(0.1, 0.45, w)
                    // A shimmer: a contour of the clouds, wandering through them.
                    acc = Math.max(sa * sa * (3 - 2 * sa), 1 - Math.abs(w - level) / 0.07) * 0.6;
                }
                return [colour, lvl, acc];
            }

            // Each pixel of an area once (every box's is the same): its place
            // from the middle kept from tick to tick, what it shows this tick.
            var drawn = {}, lastW = -1, lastH = -1, last = null;
            function areaOf(w, h) {
                if (w === lastW && h === lastH) return last;
                var key = w + "x" + h;
                lastW = w; lastH = h;
                if (drawn[key]) return (last = drawn[key]);
                var g = GEOMETRY[key];
                if (g === undefined) {
                    g = GEOMETRY[key] = { r: [], a: [] };
                    for (var y = 0; y < h; y++)
                        for (var x = 0; x < w; x++) {
                            var dx = x - (w - 1) / 2, dy = y - (h - 1) / 2;
                            g.r.push(Math.sqrt(dx * dx + dy * dy));
                            g.a.push(Math.atan2(dy, dx));
                        }
                }
                drawn[key] = last = { g: g, px: new Array(w * h) };
                return last;
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox), px;
                if (p.x >= 0 && p.x < p.w && p.y >= 0 && p.y < p.h
                        && Math.floor(p.x) === p.x && Math.floor(p.y) === p.y) {
                    var ar = areaOf(p.w, p.h), k = p.y * p.w + p.x;
                    px = ar.px[k];
                    if (px === undefined) px = ar.px[k] = pixel(ar.g.r[k], ar.g.a[k]);
                } else {
                    var dx = p.x - (p.w - 1) / 2, dy = p.y - (p.h - 1) / 2;
                    px = pixel(Math.sqrt(dx * dx + dy * dy), Math.atan2(dy, dx));
                }
                return MATRIX.out(f, cv, px[0], px[1], px[2], params);
            });
        });
    };

    return effect;
})()
