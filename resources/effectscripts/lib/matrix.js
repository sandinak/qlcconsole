/*
  QLC+ Effect Script library: Matrix

  What a pixel-matrix effect needs: the canvas its heads make (one box, or
  a whole run of boxes laid out as one picture), each head's place on it --
  on the whole canvas or inside its own box -- which heads can show a
  colour and which only white, and one way to turn "this colour, this
  bright, this much accent" into each head's intent.

  An effect says
      effect.requires = ["matrix"];
  builds the canvas once a tick and asks each head for its point:

      var cv = MATRIX.canvas(fixtures);
      return fixtures.map(function(f) {
          var p = MATRIX.point(f, cv, params.scope === 1);   // per box?
          ...
          return MATRIX.out(f, cv, colour, level, accent, params);
      });

  White pixels. A head is RGB, white, both (RGBW) or neither (a plain
  dimmer); the host says which (fixture.headRGB / fixture.headWhite). Every
  matrix effect has a "white" setting, MATRIX.whiteParams():
    0 Colour + accents  white pixels carry the white part of the colour
                        (none for a saturated red, all of it for white) and
                        the effect's accents -- a bar's leading edge, a
                        sparkle's flash
    1 Follow brightness white pixels light wherever the picture is lit
    2 Accents only      white pixels only for the accents
    3 RGB only          the white pixels left out; accents on the RGB ones
    4 White only        the picture on the white pixels; RGB ones left out
  A canvas with no white pixels takes its accents on the RGB ones; one with
  no RGB pixels (a box in its white-only mode) draws the picture in white
  whatever the setting, so nothing goes dark that could show something.

  Pure: no state, no clock but inputs, repeatable randomness from hash().
*/
var MATRIX = (function() {
    var M = {};

    M.WHITE_MODES = ["Colour + accents", "Follow brightness", "Accents only", "RGB only", "White only"];

    /** The two settings every matrix effect has for its white pixels. */
    M.whiteParams = function(defaultMode) {
        return [
            { name: "white", description: "What the white pixels do", defaultValue: defaultMode || 0,
              values: M.WHITE_MODES },
            { name: "whiteLevel", description: "White pixels' level", min: 0, max: 1, defaultValue: 1 }
        ];
    };

    /** "Whole canvas" / "Each box": draw once across every box, or the same
     *  picture in each box. */
    M.SCOPES = ["Whole canvas", "Each box"];

    function num(v, d) { return (v === undefined || v === null || isNaN(v)) ? d : Number(v); }
    M.num = num;
    M.clamp = function(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); };

    /** The canvas the heads make: its size (cells), each fixture's box on it
     *  (its cells' bounds, in left-to-right order) and what kinds of head
     *  are on it. */
    M.canvas = function(fixtures) {
        var n = fixtures.length;
        var g0 = (n > 0 && fixtures[0].grid) ? fixtures[0].grid : { cols: n, rows: 1 };
        var cv = { cols: Math.max(1, g0.cols || n), rows: Math.max(1, g0.rows || 1),
                   boxes: [], box: {}, anyRGB: false, anyWhite: false, n: n };
        for (var i = 0; i < n; i++) {
            var f = fixtures[i];
            var g = f.grid || { col: i, row: 0 };
            var b = cv.box[f.id];
            if (b === undefined) {
                b = { id: f.id, c0: g.col, c1: g.col, r0: g.row, r1: g.row, order: cv.boxes.length };
                cv.box[f.id] = b;
                cv.boxes.push(b);
            } else {
                if (g.col < b.c0) b.c0 = g.col;
                if (g.col > b.c1) b.c1 = g.col;
                if (g.row < b.r0) b.r0 = g.row;
                if (g.row > b.r1) b.r1 = g.row;
            }
            if (M.isRGB(f)) cv.anyRGB = true;
            if (f.headWhite) cv.anyWhite = true;
        }
        // Left to right (then top to bottom): the order a box chase runs in.
        var sorted = cv.boxes.slice();
        sorted.sort(function(a, b) {
            var ax = a.c0 + a.c1, bx = b.c0 + b.c1;
            if (ax !== bx) return ax - bx;
            var ay = a.r0 + a.r1, by = b.r0 + b.r1;
            if (ay !== by) return ay - by;
            return a.order - b.order;
        });
        for (var k = 0; k < sorted.length; k++) {
            sorted[k].index = k;
            sorted[k].w = sorted[k].c1 - sorted[k].c0 + 1;
            sorted[k].h = sorted[k].r1 - sorted[k].r0 + 1;
        }
        cv.boxes = sorted;
        cv.count = sorted.length;
        return cv;
    };

    /** Can this head show a colour? (Without the host's per-head word -- an
     *  older host, the effect browser's preview -- the fixture's.) */
    M.isRGB = function(f) {
        return f.headRGB !== undefined ? !!f.headRGB : (f.hasRGB !== false);
    };

    /** Where head @p f is: x, y in cells, on the whole canvas (w x h) or,
     *  with @p perBox, inside its own box; u, v the same 0..1 (0.5 on an
     *  axis one cell long); box = its box. Only a fixture that is a grid
     *  (more than one row and column of pixels) is a box to draw in: a par
     *  in a row of them, or a strip in a matrix built of strips, takes its
     *  place on the canvas whatever @p perBox says. */
    M.point = function(f, cv, perBox) {
        var g = f.grid || { col: 0, row: 0 };
        var b = cv.box[f.id];
        var p = { box: b };
        if (perBox && b && b.w > 1 && b.h > 1) {
            p.x = g.col - b.c0; p.y = g.row - b.r0; p.w = b.w; p.h = b.h;
        } else {
            p.x = g.col; p.y = g.row; p.w = cv.cols; p.h = cv.rows;
        }
        p.u = p.w > 1 ? p.x / (p.w - 1) : 0.5;
        p.v = p.h > 1 ? p.y / (p.h - 1) : 0.5;
        return p;
    };

    /** The look's colours ({r,g,b} 0..255), or @p defaults when it has none. */
    M.colours = function(palettes, defaults) {
        var L = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var out = [];
        for (var i = 0; i < L.length; i++)
            if (L[i] && L[i].r !== undefined)
                out.push({ r: num(L[i].r, 0), g: num(L[i].g, 0), b: num(L[i].b, 0) });
        return out.length > 0 ? out : (defaults || [{ r: 255, g: 255, b: 255 }]);
    };
    /** The @p k-th of @p list, round again past its end. */
    M.pick = function(list, k) {
        var n = list.length;
        return list[((Math.floor(k) % n) + n) % n];
    };
    /** Between @p a and @p b, @p t 0..1 of the way. */
    M.mix = function(a, b, t) {
        t = M.clamp(t, 0, 1);
        return { r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t };
    };
    /** Along @p list at @p u 0..1, blending neighbours (round when @p wrap). */
    M.ramp = function(list, u, wrap) {
        var n = list.length;
        if (n === 1) return list[0];
        if (wrap) {
            var x = (u - Math.floor(u)) * n, i = Math.floor(x);
            return M.mix(list[i % n], list[(i + 1) % n], x - i);
        }
        var y = M.clamp(u, 0, 1) * (n - 1), j = Math.min(n - 2, Math.floor(y));
        return M.mix(list[j], list[j + 1], y - j);
    };
    /** A hue round the colour wheel (@p h, 1 = once round), full saturation. */
    M.hue = function(h) {
        h = h - Math.floor(h);
        var i = Math.floor(h * 6), f = h * 6 - i, q = 1 - f, r, g, b;
        switch (i % 6) {
            case 0:  r = 1; g = f; b = 0; break;
            case 1:  r = q; g = 1; b = 0; break;
            case 2:  r = 0; g = 1; b = f; break;
            case 3:  r = 0; g = q; b = 1; break;
            case 4:  r = f; g = 0; b = 1; break;
            default: r = 1; g = 0; b = q; break;
        }
        return { r: r * 255, g: g * 255, b: b * 255 };
    };

    /** A repeatable "random" 0..1 for (a, b, c). */
    M.hash = function(a, b, c) {
        var x = Math.sin(a * 12.9898 + (b || 0) * 78.233 + (c || 0) * 37.719 + 0.5) * 43758.5453;
        return x - Math.floor(x);
    };
    /** Smooth repeatable noise 0..1 over (x, y), a different field per @p seed. */
    M.noise = function(x, y, seed) {
        var xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi;
        var s = seed || 0;
        var a = M.hash(xi, yi, s), b = M.hash(xi + 1, yi, s);
        var c = M.hash(xi, yi + 1, s), d = M.hash(xi + 1, yi + 1, s);
        var u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
        return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
    };
    /** 0..1 smooth step from @p e0 to @p e1. */
    M.smooth = function(e0, e1, x) {
        if (e1 === e0) return x < e0 ? 0 : 1;
        var t = M.clamp((x - e0) / (e1 - e0), 0, 1);
        return t * t * (3 - 2 * t);
    };

    /** The effect's clock: with @p onBeat and a beat running, @p perBeat
     *  steps a beat on the shared beat clock (so it lands on the beat and
     *  on every other effect's); else @p rate a second -- or a beat, on a
     *  speed master, which hands its beats over as _time. Returns
     *  { pos, beat: true|false }. */
    M.clock = function(inputs, rate, onBeat, perBeat) {
        var bpm = num(inputs._bpm, 0);
        if (onBeat && bpm > 0 && inputs._beatPos !== undefined)
            return { pos: num(inputs._beatPos, 0) * num(perBeat, 1), beat: true };
        return { pos: num(inputs._time, 0) * rate, beat: false };
    };
    M.TIMINGS = ["On the beat", "Free (speed)"];
    M.DIVISIONS = ["Every bar", "Every 2 beats", "Every beat", "Twice a beat", "4 a beat"];
    M.DIVISION_STEPS = [0.25, 0.5, 1, 2, 4];
    /** Steps a beat for the "division" setting (index into DIVISIONS). */
    M.perBeat = function(division) {
        return M.DIVISION_STEPS[M.clamp(Math.round(num(division, 2)), 0, M.DIVISION_STEPS.length - 1)];
    };

    /** A head's intent: colour @p c ({r,g,b} at full) at @p level 0..1,
     *  with @p accent 0..1 of white on top, by the white settings in
     *  @p params (see the top of this file). */
    M.out = function(f, cv, c, level, accent, params) {
        var mode = Math.round(num(params.white, 0));
        var wl = M.clamp(num(params.whiteLevel, 1), 0, 1);
        level = M.clamp(num(level, 0), 0, 1);
        accent = M.clamp(num(accent, 0), 0, 1);
        var r = c.r * level, g = c.g * level, b = c.b * level;
        var bright = Math.max(r, g, b) / 255;
        var rgbHead = M.isRGB(f), whiteHead = !!f.headWhite;
        var lit = level > 0 || accent > 0;
        var o;

        if (!rgbHead && !whiteHead) {
            // A plain dimmer pixel: the picture's brightness.
            o = { dimmer: Math.max(bright, accent) };
            return o;
        }
        if (!rgbHead) {
            // A white pixel.
            var w;
            if (!cv.anyRGB)
                w = mode === 2 ? accent : Math.max(bright, accent);
            else if (mode === 0)
                w = Math.max(Math.min(r, g, b) / 255, accent);
            else if (mode === 1 || mode === 4)
                w = Math.max(bright, accent);
            else if (mode === 2)
                w = accent;
            else
                return {};                               // RGB only: not ours
            o = { w: Math.round(255 * w * wl) };
            if (f.hasDimmer && o.w > 0) o.dimmer = 1;
            return o;
        }
        // An RGB pixel (an RGBW one if it has white too).
        if (mode === 4 && cv.anyWhite && !whiteHead)
            return {};                                   // White only: not ours
        if (mode === 4 && !cv.anyWhite) {
            // White only, and no white pixels: the picture in white.
            var gw = 255 * Math.max(bright, accent) * wl;
            r = g = b = gw;
        } else if (whiteHead) {
            // RGBW: the white part of the colour on the white emitter.
            var ww = 0;
            if (mode === 4) {
                ww = Math.max(bright, accent);
                r = g = b = 0;
            } else if (mode === 0) {
                var m = Math.min(r, g, b);
                r -= m; g -= m; b -= m;
                ww = Math.max(m / 255, accent);
            } else if (mode === 1) {
                ww = Math.max(bright, accent);
            } else if (mode === 2) {
                ww = accent;
            } else if (accent > 0) {
                r = Math.max(r, 255 * accent); g = Math.max(g, 255 * accent); b = Math.max(b, 255 * accent);
            }
            o = { r: Math.round(r), g: Math.round(g), b: Math.round(b), w: Math.round(255 * ww * wl) };
            if (f.hasDimmer && lit) o.dimmer = 1;
            return o;
        } else if (accent > 0 && (!cv.anyWhite || mode === 3)) {
            // Nowhere else for the accent to go: white on the RGB pixel.
            var a = 255 * accent;
            r = Math.max(r, a); g = Math.max(g, a); b = Math.max(b, a);
        }
        o = { r: Math.round(M.clamp(r, 0, 255)), g: Math.round(M.clamp(g, 0, 255)),
              b: Math.round(M.clamp(b, 0, 255)) };
        if (f.hasDimmer && lit) o.dimmer = 1;
        return o;
    };

    return M;
})();
