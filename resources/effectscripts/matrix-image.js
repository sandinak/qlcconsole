/*
  QLC+ Effect Script: Matrix Image
  A picture or an animated GIF, scaled onto the canvas (or into each box):
  fitted whole, filling it, or stretched. The picture is kept in the show;
  the host hands it over decoded (params.image: width, height, frames as
  RGBA ArrayBuffers, delays in ms).

  Each pixel is the average of the part of the picture it covers, so a
  detailed picture comes down to a few big pixels without sparkling. The
  averages are worked out once a frame (and canvas size) and kept.

  Pure & testable: the frame from inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Image";
    effect.description = "A picture or animated GIF on the pixels";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Plays a picture or an animated GIF on the pixels. Image: choose a file (PNG, JPEG, GIF...; up to 2 MB). It is kept in the show itself, so it goes wherever the show goes; until you choose one, a heart beats in the look's colour. Fit: the whole picture, letterboxed; filling the canvas, cropped; or stretched to it. Timing: a GIF at its own pace (Speed: 1 = as made, 2 = twice as fast), or a frame on the beat (Division: how often). Tint: blends the picture towards the look's first colour, keeping its light and shade. Highlights: the picture's near-white parts flash the white pixels (the accent; the heart's beat without a picture). Scope: one picture across the whole canvas, or the picture in each box; Each box, staggered: each box a little behind the last (Stagger order; Stagger: how far, as a fraction of the GIF).";

    effect.parameters = [
        { name: "image",    description: "The picture or animated GIF", type: "image" },
        { name: "fit",      description: "How it is scaled onto the canvas", defaultValue: 0,
          values: ["Fit (the whole picture)", "Fill (cropped)", "Stretch"] },
        { name: "timing",   description: "A GIF at its own pace, or a frame on the beat", defaultValue: 0,
          values: ["Its own timing", "A frame on the beat"] },
        { name: "division", description: "On the beat: how often it steps", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Its own timing: how fast (1 = as made)", min: 0.1, max: 4, defaultValue: 1 },
        { name: "tint",     description: "Blend towards the look's first colour", min: 0, max: 1, defaultValue: 0 },
        { name: "highlights", description: "The picture's near-white parts as accents (on the white pixels)", min: 0, max: 1, defaultValue: 0.5 },
        { name: "scope",    description: "One picture across the canvas, or one in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    function imageOf(params) {
        var img = params.image;
        return (img && typeof img === "object" && img.frames && img.frames.length > 0 && img.width > 0) ? img : null;
    }

    // Frame @p i of @p img as bytes, kept (by the picture) in @p root.
    function bytesOf(root, img, i) {
        if (root.__imgKey !== img.key) {
            root.__imgKey = img.key;
            root.__imgBytes = [];
            root.__imgCells = {};
            var at = 0;
            root.__imgEnds = [];
            for (var k = 0; k < img.frames.length; k++) {
                at += MATRIX.num(img.delays[k], 0);
                root.__imgEnds.push(at);
            }
        }
        var b = root.__imgBytes[i];
        if (b === undefined) b = root.__imgBytes[i] = new Uint8Array(img.frames[i]);
        return b;
    }

    // The colour of the picture over the w x h area's cells, for frame @p i:
    // a flat array, 4 numbers a cell (r, g, b, coverage), kept.
    function cellsOf(root, img, i, w, h, fit) {
        var key = i + ":" + w + "x" + h + ":" + fit;
        var have = root.__imgCells[key];
        if (have !== undefined) return have;
        var px = bytesOf(root, img, i);
        var W = img.width, H = img.height;
        var sx = w / W, sy = h / H;
        if (fit === 0) sx = sy = Math.min(sx, sy);
        else if (fit === 1) sx = sy = Math.max(sx, sy);
        var ox = (w - W * sx) / 2, oy = (h - H * sy) / 2;
        var out = new Array(w * h * 4);
        for (var y = 0; y < h; y++) {
            var y0 = (y - oy) / sy, y1 = (y + 1 - oy) / sy;
            for (var x = 0; x < w; x++) {
                var x0 = (x - ox) / sx, x1 = (x + 1 - ox) / sx;
                var o = (y * w + x) * 4;
                var cx0 = Math.max(0, x0), cx1 = Math.min(W, x1), cy0 = Math.max(0, y0), cy1 = Math.min(H, y1);
                if (cx1 <= cx0 || cy1 <= cy0) { out[o] = out[o + 1] = out[o + 2] = out[o + 3] = 0; continue; }
                var cover = (cx1 - cx0) * (cy1 - cy0) / ((x1 - x0) * (y1 - y0));
                var r = 0, g = 0, b = 0, wsum = 0;
                if (x1 - x0 < 1.5 && y1 - y0 < 1.5) {
                    // Bigger than the picture's own pixels: blend the four nearest.
                    var fx = MATRIX.clamp((x0 + x1) / 2 - 0.5, 0, W - 1), fy = MATRIX.clamp((y0 + y1) / 2 - 0.5, 0, H - 1);
                    var ix = Math.floor(fx), iy = Math.floor(fy), ax = fx - ix, ay = fy - iy;
                    var jx = Math.min(W - 1, ix + 1), jy = Math.min(H - 1, iy + 1);
                    var q = [[ix, iy, (1 - ax) * (1 - ay)], [jx, iy, ax * (1 - ay)], [ix, jy, (1 - ax) * ay], [jx, jy, ax * ay]];
                    for (var k = 0; k < 4; k++) {
                        var s = (q[k][1] * W + q[k][0]) * 4;
                        r += px[s] * q[k][2]; g += px[s + 1] * q[k][2]; b += px[s + 2] * q[k][2];
                    }
                    wsum = 1;
                } else {
                    // Smaller: the average of every picture pixel it covers, part ones in part.
                    for (var yy = Math.floor(cy0); yy < cy1; yy++) {
                        var wy = Math.min(cy1, yy + 1) - Math.max(cy0, yy);
                        for (var xx = Math.floor(cx0); xx < cx1; xx++) {
                            var wgt = wy * (Math.min(cx1, xx + 1) - Math.max(cx0, xx));
                            var si = (yy * W + xx) * 4;
                            r += px[si] * wgt; g += px[si + 1] * wgt; b += px[si + 2] * wgt;
                            wsum += wgt;
                        }
                    }
                }
                out[o] = wsum > 0 ? r / wsum : 0;
                out[o + 1] = wsum > 0 ? g / wsum : 0;
                out[o + 2] = wsum > 0 ? b / wsum : 0;
                out[o + 3] = cover;
            }
        }
        root.__imgCells[key] = out;
        return out;
    }

    // No picture yet: a heart, beating, in the look's first colour.
    function heart(p, beat) {
        var size = (Math.min(p.h, p.w) / 2 + 0.3) * (1 + 0.12 * Math.pow(1 - beat, 3));
        var cx = (p.w - 1) / 2, cy = (p.h - 1) / 2 - 0.1;
        var hits = 0;
        for (var sy = 0; sy < 3; sy++)
            for (var sx = 0; sx < 3; sx++) {
                var x = (p.x - 0.33 + sx * 0.33 - cx) / size * 1.25;
                var y = -(p.y - 0.33 + sy * 0.33 - cy) / size * 1.25 + 0.15;
                var a = x * x + y * y - 1;
                if (a * a * a - x * x * y * y * y <= 0) hits++;
            }
        return hits / 9;
    }

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var img = imageOf(params);
        var onBeat = Math.round(num(params.timing, 0)) === 1;
        var speed = Math.max(0.01, num(params.speed, 1));
        var perBeat = MATRIX.perBeat(params.division);
        var frames = img ? img.frames.length : 1;
        // A cycle: once through the GIF (one beat for the heart).
        var cyc = img ? MATRIX.cycle(frames, onBeat ? 1 : frames * 1000 / Math.max(1, img.totalMs) * speed, perBeat)
                      : MATRIX.cycle(1, 2, 1);
        return MATRIX.scoped(cv, fixtures, inputs, params, state, cyc, function(fixtures, inputs, st, root) {
            var t = num(inputs._time, 0);
            var fit = MATRIX.clamp(Math.round(num(params.fit, 0)), 0, 2);
            var tint = MATRIX.clamp(num(params.tint, 0), 0, 1);
            var hl = MATRIX.clamp(num(params.highlights, 0.5), 0, 1);
            var perBox = MATRIX.perBox(params, 0);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 20, b: 60 }]);

            if (!img) {
                var bpm = num(inputs._bpm, 0);
                var beat = bpm > 0 ? num(inputs._beat, 0) : (t * 2) - Math.floor(t * 2);
                return fixtures.map(function(f) {
                    var p = MATRIX.point(f, cv, perBox);
                    var lit = heart(p, beat);
                    return MATRIX.out(f, cv, C[0], lit * (0.7 + 0.3 * Math.pow(1 - beat, 2)),
                                      lit * hl * Math.pow(1 - beat, 4), params);
                });
            }

            // Which frame.
            var fi = 0;
            bytesOf(root, img, 0);
            if (frames > 1) {
                if (onBeat) {
                    var clk = MATRIX.clock(inputs, speed * frames * 1000 / Math.max(1, img.totalMs), true, perBeat);
                    fi = ((Math.floor(clk.pos) % frames) + frames) % frames;
                } else {
                    var total = Math.max(1, img.totalMs);
                    var ms = t * 1000 * speed;
                    ms = ms - Math.floor(ms / total) * total;
                    var ends = root.__imgEnds, lo = 0, hi = frames - 1;
                    while (lo < hi) {
                        var mid = (lo + hi) >> 1;
                        if (ends[mid] > ms) hi = mid; else lo = mid + 1;
                    }
                    fi = lo;
                }
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var cells = cellsOf(root, img, fi, p.w, p.h, fit);
                var o = (p.y * p.w + p.x) * 4;
                var colour = { r: cells[o], g: cells[o + 1], b: cells[o + 2] };
                if (tint > 0) {
                    var lum = (0.3 * colour.r + 0.59 * colour.g + 0.11 * colour.b) / 255;
                    colour = MATRIX.mix(colour, { r: C[0].r * lum, g: C[0].g * lum, b: C[0].b * lum }, tint);
                }
                var bright = Math.max(colour.r, colour.g, colour.b);
                if (bright <= 0 || cells[o + 3] <= 0) return MATRIX.out(f, cv, colour, 0, 0, params);
                // Full colour at the picture's own brightness, as a level.
                var full = { r: colour.r * 255 / bright, g: colour.g * 255 / bright, b: colour.b * 255 / bright };
                var acc = hl > 0 ? hl * MATRIX.smooth(0.75, 1, Math.min(colour.r, colour.g, colour.b) / 255) * cells[o + 3] : 0;
                return MATRIX.out(f, cv, full, bright / 255 * cells[o + 3], acc, params);
            });
        });
    };

    return effect;
})()
