/*
  QLC+ Effect Script: Matrix Snake
  Snakes of light wind through the matrix along a path -- back and forth
  along the rows, up and down the columns, spiralling in, or round the
  edge -- in each box, or through the whole canvas.

  Pure & testable: driven by inputs._time; the path is worked out once per
  size and kept in state.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Snake";
    effect.description = "Snakes of light wind through the matrix along a path";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Snakes run along a path through the pixels: back and forth along the rows, up and down the columns, spiralling into the middle, or round the edge. Snakes: how many follow each other round; Length: each one's length in pixels, its tail fading. Colours: one of the look's colours each, or a rainbow along the body. Speed: pixels a second (per beat on a speed master). Scope: a path in each box, or one through the whole canvas. The accent is each snake's head.";

    effect.parameters = [
        { name: "speed",   description: "Pixels a second (per beat on a speed master)", min: 1, max: 80, defaultValue: 20 },
        { name: "path",    description: "The way through the pixels", defaultValue: 0,
          values: ["Along the rows", "Up and down the columns", "Spiral in", "Round the edge"] },
        { name: "snakes",  description: "Snakes following each other round", min: 1, max: 6, defaultValue: 2 },
        { name: "length",  description: "Each snake's length (pixels)", min: 1, max: 40, defaultValue: 8 },
        { name: "colours", description: "Colouring", defaultValue: 0, values: ["A look colour each", "Rainbow along the body"] },
        { name: "scope",   description: "A path in each box, or through the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.whiteParams(0));

    // Each pixel's place along the path through a w x h area, -1 when the
    // path misses it (the edge path's middle). { order: [], length: n }
    function buildPath(w, h, kind) {
        var order = [], n = 0, x, y;
        for (var i = 0; i < w * h; i++) order.push(-1);
        function visit(px, py) { if (order[py * w + px] < 0) order[py * w + px] = n++; }
        if (kind === 1) {
            for (x = 0; x < w; x++)
                for (var j = 0; j < h; j++) visit(x, (x & 1) ? h - 1 - j : j);
        } else if (kind === 2) {
            var x0 = 0, x1 = w - 1, y0 = 0, y1 = h - 1;
            while (x0 <= x1 && y0 <= y1) {
                for (x = x0; x <= x1; x++) visit(x, y0);
                for (y = y0 + 1; y <= y1; y++) visit(x1, y);
                if (y1 > y0) for (x = x1 - 1; x >= x0; x--) visit(x, y1);
                if (x1 > x0) for (y = y1 - 1; y > y0; y--) visit(x0, y);
                x0++; x1--; y0++; y1--;
            }
        } else if (kind === 3) {
            for (x = 0; x < w; x++) visit(x, 0);
            for (y = 1; y < h; y++) visit(w - 1, y);
            for (x = w - 2; x >= 0 && h > 1; x--) visit(x, h - 1);
            for (y = h - 2; y > 0 && w > 1; y--) visit(0, y);
        } else {
            for (y = 0; y < h; y++)
                for (var k = 0; k < w; k++) visit((y & 1) ? w - 1 - k : k, y);
        }
        return { order: order, length: Math.max(1, n) };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var kind = Math.round(num(params.path, 0));
        var snakes = Math.max(1, Math.round(num(params.snakes, 2)));
        var len = Math.max(1, num(params.length, 8));
        var rainbow = Math.round(num(params.colours, 0)) === 1;
        var perBox = Math.round(num(params.scope, 1)) === 1;
        var C = MATRIX.colours(palettes, [{ r: 0, g: 255, b: 120 }, { r: 255, g: 0, b: 140 }]);
        var travelled = num(inputs._time, 0) * num(params.speed, 20);
        if (state.paths === undefined) state.paths = {};

        return fixtures.map(function(f) {
            var p = MATRIX.point(f, cv, perBox);
            var key = p.w + "x" + p.h + ":" + kind;
            var path = state.paths[key];
            if (path === undefined) path = state.paths[key] = buildPath(p.w, p.h, kind);
            var at = path.order[p.y * p.w + p.x];
            if (at === undefined || at < 0) return MATRIX.out(f, cv, C[0], 0, 0, params);
            var L = path.length;
            var gapEach = L / snakes;
            var best = 0, acc = 0, which = 0, along = 0;
            for (var s = 0; s < snakes; s++) {
                var head = travelled + s * gapEach;
                var behind = head - at;
                behind -= Math.floor(behind / L) * L;           // 0..L
                if (behind < len) {
                    var lvl = 1 - behind / len;
                    lvl = 0.15 + 0.85 * lvl;
                    if (lvl > best) { best = lvl; which = s; along = behind / len; }
                    if (behind < 1) acc = Math.max(acc, 1 - behind);
                }
            }
            var colour = rainbow ? MATRIX.hue(along * 0.85 + which / snakes) : MATRIX.pick(C, which);
            return MATRIX.out(f, cv, colour, best, acc, params);
        });
    };

    return effect;
})()
