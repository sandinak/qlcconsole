/*
  QLC+ Effect Script: Matrix Bars
  Bars of colour scan across a pixel matrix -- left to right, up, down, or
  bouncing between the edges -- over the whole canvas (a run of boxes as
  one picture) or in each box. The classic box-matrix scan.

  Pure & testable: driven by inputs._time; no Math.random / wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Bars";
    effect.description = "Bars of colour scan across the matrix, or bounce between its edges";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Bars of the look's colours (one each, in turn) sweep across the pixels. Direction: across, up, down, or bouncing. Bars: how many share the canvas. Width: each bar's width in pixels; Tail: how far it trails off behind (0 = a hard bar). Scope: one sweep across the whole canvas, or the same sweep in each box. Speed: sweeps a second (per beat on a speed master). White pixels: see the white setting; the accent is each bar's leading edge. Each box, staggered: the same in every box, each a little behind the last, so it rolls across the rig -- Stagger order: left to right, right to left, out from the middle or random; Stagger: how far behind, as a fraction of a sweep.";

    effect.parameters = [
        { name: "speed",     description: "Sweeps per second (per beat on a speed master)", min: 0.05, max: 4, defaultValue: 0.5 },
        { name: "direction", description: "Which way the bars go", defaultValue: 0,
          values: ["Left to right", "Right to left", "Top to bottom", "Bottom to top", "Bounce across", "Bounce up and down"] },
        { name: "bars",      description: "Bars on the canvas", min: 1, max: 8, defaultValue: 1 },
        { name: "width",     description: "Bar width (pixels)", min: 1, max: 20, defaultValue: 2 },
        { name: "tail",      description: "Trail behind each bar (0 = a hard bar, 1 = to the next)", min: 0, max: 1, defaultValue: 0.3 },
        { name: "scope",     description: "Across the whole canvas, or in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.5)),
                             function(fixtures, inputs, state, root) {
            var num = MATRIX.num;
            var dir = Math.round(num(params.direction, 0));
            var bars = Math.max(1, Math.round(num(params.bars, 1)));
            var width = Math.max(0.5, num(params.width, 2));
            var tail = MATRIX.clamp(num(params.tail, 0.3), 0, 1);
            var perBox = MATRIX.perBox(params, 0);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 40, b: 0 }, { r: 0, g: 80, b: 255 }]);
            var phase = num(inputs._time, 0) * num(params.speed, 0.5);
            var across = (dir === 0 || dir === 1 || dir === 4);
            var bounce = dir >= 4;

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var L = across ? p.w : p.h;
                var s = across ? p.x : p.y;
                if (dir === 1 || dir === 3) s = L - 1 - s;
                var best = 0, edge = 0, which = 0;
                for (var k = 0; k < bars; k++) {
                    var lvl, acc;
                    if (bounce) {
                        var q = phase + k / bars;
                        q = q - Math.floor(q);
                        var tri = q < 0.5 ? q * 2 : 2 - q * 2;          // 0..1..0
                        var centre = tri * Math.max(0, L - 1);
                        var d = Math.abs(s - centre);
                        var half = width / 2;
                        var soft = Math.max(0.01, tail * L / bars / 2);
                        lvl = d <= half ? 1 : Math.max(0, 1 - (d - half) / soft);
                        if (tail === 0) lvl = d <= half ? 1 : 0;
                        acc = d < 0.75 ? 1 - d / 0.75 : 0;
                    } else {
                        var span = L + width;                          // so a bar leaves before it comes back
                        var head = (phase + k / bars) * span;
                        var back = head - s;                           // how far behind the bar's front
                        back = back - Math.floor(back / span) * span;  // 0..span
                        var trail = tail * span / bars;
                        if (back < width) lvl = 1;
                        else if (trail > 0 && back < width + trail) lvl = 1 - (back - width) / trail;
                        else lvl = 0;
                        acc = back < 1 ? 1 - back : 0;
                    }
                    if (lvl > best) { best = lvl; which = k; }
                    if (acc > edge) edge = acc;
                }
                return MATRIX.out(f, cv, MATRIX.pick(C, which), best, edge * (best > 0 ? 1 : 0), params);
            });
        });
    };

    return effect;
})()
