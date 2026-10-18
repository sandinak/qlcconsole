/*
  QLC+ Effect Script: Matrix Rain
  Drops of light fall down the pixel columns, each with a fading trail --
  digital rain. Each column has its own drops at their own pace, the same
  every time it runs.

  Pure & testable: driven by inputs._time; randomness from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Rain";
    effect.description = "Drops of light fall down the pixel columns, trailing off";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Drops fall down every column of the canvas, each column at its own pace, with a trail fading behind. Colours: each column takes one of the look's colours (code green with none). Density: how many drops there are; Trail: how long each one's tail is; Speed: pixels a second (per beat on a speed master); Direction: down, up, or sideways along the rows. The accent is each drop's head -- on a box with white pixels, a white glint at the front of each drop.";

    effect.parameters = [
        { name: "speed",     description: "Pixels a second (per beat on a speed master)", min: 0.5, max: 40, defaultValue: 6 },
        { name: "density",   description: "How many drops", min: 0.05, max: 1, defaultValue: 0.4 },
        { name: "trail",     description: "Trail length (pixels)", min: 0, max: 12, defaultValue: 3 },
        { name: "direction", description: "Which way the drops fall", defaultValue: 0,
          values: ["Down", "Up", "Left to right", "Right to left"] }
    ].concat(MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var t = num(inputs._time, 0);
        var speed = Math.max(0.1, num(params.speed, 6));
        var density = MATRIX.clamp(num(params.density, 0.4), 0.01, 1);
        var trail = Math.max(0, num(params.trail, 3));
        var dir = Math.round(num(params.direction, 0));
        var C = MATRIX.colours(palettes, [{ r: 0, g: 255, b: 70 }]);
        var vertical = dir < 2;
        var len = vertical ? cv.rows : cv.cols;
        // A drop's cycle: across the canvas, its trail, then a gap that
        // shrinks as density rises.
        var gap = (1 - density) * len * 3 + 1;
        var cycle = len + trail + gap;
        var lanes = 2;                                   // drops a column can have at once

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var lane = vertical ? g.col : g.row;
            var s = vertical ? g.row : g.col;
            if (dir === 1 || dir === 3) s = len - 1 - s;
            var best = 0, acc = 0, colourOf = 0;
            for (var j = 0; j < lanes; j++) {
                var pace = 0.6 + 0.8 * MATRIX.hash(lane, j, 11);
                var head = t * speed * pace + MATRIX.hash(lane, j, 12) * cycle + j * cycle / lanes;
                var n = Math.floor(head / cycle);
                head -= n * cycle;                       // where this drop is in its cycle
                var behind = head - s;
                if (behind < 0 || behind > trail + 1) continue;
                var lvl = behind < 1 ? 1 : (trail > 0 ? Math.max(0, 1 - (behind - 1) / trail) : 0);
                lvl *= lvl;
                if (lvl > best) { best = lvl; colourOf = Math.floor(MATRIX.hash(lane, n, j + 13) * 997); }
                if (behind < 1) acc = Math.max(acc, 1 - behind);
            }
            return MATRIX.out(f, cv, MATRIX.pick(C, colourOf), best, acc, params);
        });
    };

    return effect;
})()
