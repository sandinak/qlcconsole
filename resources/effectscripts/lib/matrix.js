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

    /** "Whole canvas" / "Each box" / "Each box, staggered": draw once across
     *  every box, the same picture in each box, or the same picture in each
     *  box a little later box to box (see scoped()). */
    M.SCOPES = ["Whole canvas", "Each box", "Each box, staggered"];
    /** The order a staggered picture runs through the boxes. */
    M.STAGGER_ORDERS = ["Left to right", "Right to left", "Out from the middle", "Random"];
    /** The two settings that go with a scope: which way the stagger runs
     *  and how far behind each box is, as a fraction of the effect's cycle. */
    M.staggerParams = function() {
        return [
            { name: "staggerOrder", description: "Staggered: the order the boxes go in", defaultValue: 0,
              values: M.STAGGER_ORDERS },
            { name: "stagger", description: "Staggered: each box behind the last (fraction of a cycle)",
              min: 0, max: 1, defaultValue: 0.125 }
        ];
    };

    function num(v, d) { return (v === undefined || v === null || isNaN(v)) ? d : Number(v); }
    M.num = num;
    M.clamp = function(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); };

    /** The scope setting: 0 whole canvas, 1 each box, 2 each box staggered. */
    M.scope = function(params, dflt) {
        return M.clamp(Math.round(num(params.scope, dflt || 0)), 0, 2);
    };
    /** Draw in each box (either kind of "each box")? */
    M.perBox = function(params, dflt) { return M.scope(params, dflt) >= 1; };

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
            if (f.headRGB !== undefined ? !!f.headRGB : (f.hasRGB !== false)) cv.anyRGB = true;   // M.isRGB(f)
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
        var x, y, w, h;
        if (perBox && b && b.w > 1 && b.h > 1) {
            x = g.col - b.c0; y = g.row - b.r0; w = b.w; h = b.h;
        } else {
            x = g.col; y = g.row; w = cv.cols; h = cv.rows;
        }
        // One literal, not a property at a time: this runs for every head.
        return { box: b, x: x, y: y, w: w, h: h,
                 u: w > 1 ? x / (w - 1) : 0.5, v: h > 1 ? y / (h - 1) : 0.5 };
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

    // The white settings as out() last worked them out: the same for every
    // head of a tick, so read once a tick, not once a head.
    var outParams = null, outWhite, outWhiteLevel, outMode = 0, outWl = 1;
    /** A head's intent: colour @p c ({r,g,b} at full) at @p level 0..1,
     *  with @p accent 0..1 of white on top, by the white settings in
     *  @p params (see the top of this file). */
    M.out = function(f, cv, c, level, accent, params) {
        var pw = params.white, pl = params.whiteLevel;
        if (params !== outParams || pw !== outWhite || pl !== outWhiteLevel) {
            outParams = params; outWhite = pw; outWhiteLevel = pl;
            outMode = Math.round(num(pw, 0));
            outWl = M.clamp(num(pl, 1), 0, 1);
        }
        var mode = outMode, wl = outWl;
        // num() and clamp() written out: this runs for every head.
        if (typeof level !== "number" || level !== level) level = num(level, 0);
        level = level < 0 ? 0 : (level > 1 ? 1 : level);
        if (typeof accent !== "number" || accent !== accent) accent = num(accent, 0);
        accent = accent < 0 ? 0 : (accent > 1 ? 1 : accent);
        var r = c.r * level, g = c.g * level, b = c.b * level;
        var bright = Math.max(r, g, b) / 255;
        var rgbHead = f.headRGB !== undefined ? !!f.headRGB : (f.hasRGB !== false);   // M.isRGB(f)
        var whiteHead = !!f.headWhite;
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
        o = { r: Math.round(r < 0 ? 0 : (r > 255 ? 255 : r)), g: Math.round(g < 0 ? 0 : (g > 255 ? 255 : g)),
              b: Math.round(b < 0 ? 0 : (b > 255 ? 255 : b)) };
        if (f.hasDimmer && lit) o.dimmer = 1;
        return o;
    };

    /** One cycle of an effect -- a sweep, a ring, a turn, a step or two --
     *  as so much _time (seconds, or beats on a speed master) and so many
     *  beats on the beat clock: @p steps of @p rate a second, or of
     *  @p perBeat a beat. What a staggered box lags by. */
    M.cycle = function(steps, rate, perBeat) {
        var s = num(steps, 1);
        var r = Math.max(1e-6, Math.abs(num(rate, 1)));
        var b = Math.max(1e-6, Math.abs(num(perBeat, r)));
        return { seconds: s / r, beats: s / b };
    };

    /** @p inputs as they were @p lag cycles (@p cyc, from cycle()) ago. */
    M.lagInputs = function(inputs, lag, cyc) {
        var o = {};
        for (var k in inputs) o[k] = inputs[k];
        if (lag === 0) return o;
        o._time = num(inputs._time, 0) - lag * cyc.seconds;
        var bp;
        if (inputs._beatPos !== undefined) bp = num(inputs._beatPos, 0);
        else if (inputs._beat !== undefined) bp = num(inputs._beatCount, 0) + num(inputs._beat, 0);
        if (bp !== undefined) {
            bp -= lag * cyc.beats;
            o._beatPos = bp;
            o._beat = bp - Math.floor(bp);
            o._beatCount = Math.floor(bp);
        }
        return o;
    };

    /** Each grid box's place in a staggered run (0 first), by @p order
     *  (STAGGER_ORDERS), as { boxIndex: rank }. Only a box that is a grid
     *  is staggered; the rest draw with the whole canvas, unshifted. */
    M.staggerRanks = function(cv, order) {
        var grid = [], i;
        for (i = 0; i < cv.boxes.length; i++)
            if (cv.boxes[i].w > 1 && cv.boxes[i].h > 1) grid.push(cv.boxes[i]);
        var n = grid.length, ranks = {};
        if (order === 3) {
            // Random, but the same every tick: shuffled by a hash of the rig.
            var keyed = grid.map(function(b, k) { return { b: b, h: M.hash(k + 1, n, 7) }; });
            keyed.sort(function(a, c) { return a.h - c.h; });
            for (i = 0; i < n; i++) ranks[keyed[i].b.index] = i;
            return ranks;
        }
        for (i = 0; i < n; i++) {
            var r = i;
            if (order === 1) r = n - 1 - i;
            else if (order === 2) r = Math.floor(Math.abs(i - (n - 1) / 2));
            ranks[grid[i].index] = r;
        }
        return ranks;
    };

    /** Draw by the scope. @p draw(fixtures, inputs, state, root) draws the
     *  heads it is given and returns their intents. Whole canvas or each
     *  box: once, as it is. Each box, staggered: once per place in the run,
     *  the boxes in each place with the inputs moved back by their lag
     *  (staggerOrder, stagger x @p cyc) -- the same picture in every box,
     *  rolling from box to box. Each place keeps its own state (what a
     *  stateful effect remembers is its own moment's); @p root is the
     *  effect's state, for what every place shares. */
    M.scoped = function(cv, fixtures, inputs, params, state, cyc, draw) {
        var off = M.clamp(num(params.stagger, 0.125), 0, 1);
        if (M.scope(params, 0) !== 2 || off === 0)
            return draw(fixtures, inputs, state, state);
        var ranks = M.staggerRanks(cv, Math.round(num(params.staggerOrder, 0)));
        var groups = {}, keys = [], i;
        for (i = 0; i < fixtures.length; i++) {
            var b = cv.box[fixtures[i].id];
            var r = (b && ranks[b.index] !== undefined) ? ranks[b.index] : -1;
            if (groups[r] === undefined) { groups[r] = []; keys.push(r); }
            groups[r].push(i);
        }
        if (keys.length < 2)
            return draw(fixtures, inputs, state, state);
        if (state.__staggered === undefined) state.__staggered = {};
        var out = new Array(fixtures.length);
        for (var g = 0; g < keys.length; g++) {
            var key = keys[g], idx = groups[key];
            var fx = new Array(idx.length);
            for (i = 0; i < idx.length; i++) fx[i] = fixtures[idx[i]];
            var st = state.__staggered[key];
            if (st === undefined) st = state.__staggered[key] = {};
            var res = draw(fx, M.lagInputs(inputs, key < 0 ? 0 : key * off, cyc), st, state);
            for (i = 0; i < idx.length; i++) out[idx[i]] = res[i];
        }
        return out;
    };

    /** Keep a little history of @p value (the live audio, say) in @p root
     *  under @p key, at time @p t, @p keep long: so a staggered box can show
     *  what was playing when its moment was. */
    M.record = function(root, key, t, value, keep) {
        var h = root[key];
        if (h === undefined || (h.length && t < h[h.length - 1].t)) h = root[key] = [];
        if (h.length && h[h.length - 1].t === t) h[h.length - 1].v = value;
        else h.push({ t: t, v: value });
        var limit = t - num(keep, 6);
        var drop = 0;
        while (drop < h.length - 1 && h[drop + 1].t <= limit) drop++;
        if (drop > 0) h.splice(0, drop);
    };
    /** The value recorded under @p key as it was at @p t (the last one at or
     *  before it; the oldest one before history starts). */
    M.recalled = function(root, key, t) {
        var h = root[key];
        if (h === undefined || h.length === 0) return undefined;
        var lo = 0, hi = h.length - 1;
        if (t >= h[hi].t) return h[hi].v;
        if (t <= h[0].t) return h[0].v;
        while (hi - lo > 1) {
            var mid = (lo + hi) >> 1;
            if (h[mid].t <= t) lo = mid; else hi = mid;
        }
        return h[lo].v;
    };

    /** 0..1 how much of a pixel a soft line or dot covers, @p d pixels from
     *  its middle, @p width pixels wide: one pixel's worth of edge, so a thin
     *  line on big pixels glides rather than steps. */
    M.cover = function(d, width) {
        return M.clamp(width / 2 + 0.5 - d, 0, 1);
    };
    /** Distance from (@p px, @p py) to the segment (@p ax, @p ay)-(@p bx, @p by),
     *  and how far along it the nearest point is (0..1), as { d, t }. */
    M.segment = function(px, py, ax, ay, bx, by) {
        var vx = bx - ax, vy = by - ay;
        var l2 = vx * vx + vy * vy;
        var t = l2 > 0 ? M.clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0, 1) : 0;
        var dx = px - (ax + vx * t), dy = py - (ay + vy * t);
        return { d: Math.sqrt(dx * dx + dy * dy), t: t };
    };
    /** Is the look giving colours? (Effects with a palette of their own use
     *  theirs when it isn't.) */
    M.lookHasColours = function(palettes) {
        var L = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        for (var i = 0; i < L.length; i++)
            if (L[i] && L[i].r !== undefined) return true;
        return false;
    };

    return M;
})();
