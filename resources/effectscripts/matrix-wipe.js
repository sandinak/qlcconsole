/*
  QLC+ Effect Script: Matrix Wipe
  Colour wipes across the matrix and away again -- left to right, from the
  middle out, diagonally, as a growing circle -- or wipes each of the
  look's colours over the last in turn.

  Pure & testable: driven by inputs._time / the beat clock; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Wipe";
    effect.description = "Colour wipes on and off across the matrix";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "The matrix fills with colour and empties again, the edge travelling the way you choose: across, up or down, out from the middle, in from the edges, diagonally, or as a circle from the middle. Cycle: fill and empty the same way (it follows itself across), fill and empty back the way it came, or wipe each of the look's colours over the one before. Timing: free at Speed wipes a second, or one on the beat (Division: how often). Edge: how soft the wipe's edge is. Scope: across the whole canvas, or in each box. The accent is the moving edge.";

    effect.parameters = [
        { name: "timing",    description: "On the beat, or free at Speed", defaultValue: 1, values: MATRIX.TIMINGS },
        { name: "division",  description: "On the beat: how often it wipes", defaultValue: 0, values: MATRIX.DIVISIONS },
        { name: "speed",     description: "Free: wipes a second (per beat on a speed master)", min: 0.05, max: 4, defaultValue: 0.5 },
        { name: "direction", description: "Which way the edge travels", defaultValue: 0,
          values: ["Left to right", "Right to left", "Top to bottom", "Bottom to top", "Out from the middle",
                   "In from the edges", "Diagonal", "Circle out"] },
        { name: "cycle",     description: "What happens each time round", defaultValue: 0,
          values: ["Fill, then empty the same way", "Fill, then empty back", "Each colour over the last"] },
        { name: "edge",      description: "Softness of the wipe's edge", min: 0, max: 1, defaultValue: 0.15 },
        { name: "scope",     description: "Across the whole canvas, or in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var onBeat = Math.round(num(params.timing, 1)) === 0;
        var clk = MATRIX.clock(inputs, num(params.speed, 0.5), onBeat, MATRIX.perBeat(params.division));
        var dir = Math.round(num(params.direction, 0));
        var cycle = Math.round(num(params.cycle, 0));
        var soft = MATRIX.clamp(num(params.edge, 0.15), 0, 1) * 0.5 + 0.001;
        var perBox = Math.round(num(params.scope, 0)) === 1;
        var C = MATRIX.colours(palettes, [{ r: 0, g: 180, b: 255 }, { r: 255, g: 0, b: 90 }]);
        var black = { r: 0, g: 0, b: 0 };
        var n = Math.floor(clk.pos), fr = clk.pos - n;

        // How far along the wipe's travel each pixel is, 0..1.
        function along(p) {
            var dx = p.w > 1 ? p.x / (p.w - 1) : 0.5, dy = p.h > 1 ? p.y / (p.h - 1) : 0.5;
            switch (dir) {
                case 1: return 1 - dx;
                case 2: return dy;
                case 3: return 1 - dy;
                case 4: return Math.abs(dx - 0.5) * 2;
                case 5: return 1 - Math.abs(dx - 0.5) * 2;
                case 6: return (dx * (p.w - 1) + dy * (p.h - 1)) / Math.max(1, (p.w - 1) + (p.h - 1));
                case 7: {
                    var ex = (dx - 0.5) * (p.w - 1), ey = (dy - 0.5) * (p.h - 1);
                    var R = 0.5 * Math.sqrt((p.w - 1) * (p.w - 1) + (p.h - 1) * (p.h - 1));
                    return R > 0 ? Math.sqrt(ex * ex + ey * ey) / R : 0;
                }
                default: return dx;
            }
        }
        // 0..1 how far the edge has passed @p s, an edge at @p e moving forward.
        function covered(s, e) {
            var x = (e * (1 + soft) - s) / soft;
            return MATRIX.clamp(x, 0, 1);
        }

        return fixtures.map(function(f) {
            var p = MATRIX.point(f, cv, perBox);
            var s = along(p);
            var colour, lvl, acc = 0, cov;
            if (cycle === 2) {
                cov = covered(s, fr);
                colour = MATRIX.mix(C.length > 1 ? MATRIX.pick(C, n - 1) : black, MATRIX.pick(C, n), cov);
                lvl = (C.length > 1) ? 1 : cov;
                if (C.length === 1 && (n & 1)) lvl = 1 - cov;    // one colour: on, then off
                acc = cov > 0 && cov < 1 ? 1 - Math.abs(cov - 0.5) * 2 : 0;
            } else {
                // First half fills, second empties.
                var filling = fr < 0.5;
                var e = filling ? fr * 2 : (fr - 0.5) * 2;
                cov = covered(s, e);
                if (filling) lvl = cov;
                else if (cycle === 1) lvl = 1 - covered(1 - s, e);
                else lvl = 1 - cov;
                colour = MATRIX.pick(C, n);
                acc = (cov > 0 && cov < 1) ? 1 - Math.abs(cov - 0.5) * 2 : 0;
                if (cycle === 1 && !filling) {
                    var c2 = covered(1 - s, e);
                    acc = (c2 > 0 && c2 < 1) ? 1 - Math.abs(c2 - 0.5) * 2 : 0;
                }
            }
            return MATRIX.out(f, cv, colour, lvl, acc, params);
        });
    };

    return effect;
})()
