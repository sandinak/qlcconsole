/*
  QLC+ Effect Script: Matrix Gradient Sweep
  A gradient of the look's colours, laid at any angle, sweeping across the
  canvas -- smooth blends, or bands with soft edges.

  Pure & testable: driven by inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Gradient Sweep";
    effect.description = "The look's colours as a gradient, sweeping at an angle";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "The look's colours blend into one another in a gradient laid across the canvas at the angle you set, and sweep along it. Angle: which way the gradient runs (0 = left to right, 90 = top to bottom). Width: how long one run through all the colours is, in canvas lengths (0.5 = twice across the canvas, 2 = half of it). Speed: runs a second sweeping past (minus = the other way; per beat on a speed master). Blend: smooth, or bands of each colour with soft edges. Sheen: a line of white riding the sweep where each run starts (the accent). With one colour in the look, it sweeps from it to dark. Scope: across the whole canvas, or the same gradient in each box; Each box, staggered: each box a little behind the last (Stagger order; Stagger: how far, as a fraction of a run).";

    effect.parameters = [
        { name: "angle", description: "Which way the gradient runs", type: "direction", min: 0, max: 360, defaultValue: 0 },
        { name: "width", description: "One run through the colours (canvas lengths)", min: 0.1, max: 4, defaultValue: 1 },
        { name: "speed", description: "Runs a second sweeping past (minus = the other way)", min: -2, max: 2, defaultValue: 0.25 },
        { name: "blend", description: "Smooth, or bands of colour", defaultValue: 0, values: ["Smooth", "Bands"] },
        { name: "sheen", description: "A line of white riding the sweep, once a run", min: 0, max: 1, defaultValue: 0.4 },
        { name: "scope", description: "Across the whole canvas, or in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.25)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var rad = num(params.angle, 0) * Math.PI / 180;
            var ux = Math.cos(rad), uy = Math.sin(rad);
            var width = Math.max(0.05, num(params.width, 1));
            var bands = Math.round(num(params.blend, 0)) === 1;
            var perBox = MATRIX.perBox(params, 0);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 120 }, { r: 255, g: 150, b: 0 }, { r: 0, g: 160, b: 255 }]);
            var one = C.length === 1;
            var shift = num(inputs._time, 0) * num(params.speed, 0.25);
            var sheen = MATRIX.clamp(num(params.sheen, 0.4), 0, 1);

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var along = (p.x - (p.w - 1) / 2) * ux + (p.y - (p.h - 1) / 2) * uy;
                var span = Math.abs(ux) * Math.max(1, p.w - 1) + Math.abs(uy) * Math.max(1, p.h - 1);
                var u = along / (span * width) - shift;
                var colour, lvl = 1;
                if (one) {
                    // One colour: from it to dark and back.
                    colour = C[0];
                    lvl = bands ? MATRIX.smooth(0.2, 0.3, Math.abs((u - Math.floor(u)) - 0.5))
                                : 0.5 + 0.5 * Math.cos(2 * Math.PI * u);
                } else if (bands) {
                    var x = (u - Math.floor(u)) * C.length;
                    var i = Math.floor(x), fx = x - i;
                    // A soft edge where one band meets the next.
                    var edge = Math.min(0.5, 1.5 / Math.max(1, span * width / C.length));
                    colour = fx > 1 - edge ? MATRIX.mix(MATRIX.pick(C, i), MATRIX.pick(C, i + 1), (fx - (1 - edge)) / edge)
                                           : MATRIX.pick(C, i);
                } else colour = MATRIX.ramp(C, u, true);
                // The sheen: where each run starts, a pixel wide.
                var acc = sheen > 0 ? sheen * MATRIX.cover(Math.abs(u - Math.round(u)) * span * width, 1) : 0;
                return MATRIX.out(f, cv, colour, lvl, acc, params);
            });
        });
    };

    return effect;
})()
