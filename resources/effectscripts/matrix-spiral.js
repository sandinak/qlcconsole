/*
  QLC+ Effect Script: Matrix Spiral
  Spiral arms wind round the middle of each box (or the whole canvas) and
  turn, a pinwheel of the look's colours.

  Pure & testable: driven by inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Spiral";
    effect.description = "Spiral arms turn round the middle like a pinwheel";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Arms of colour spiral out from the middle and turn, each in the next of the look's colours. Arms: how many. Tightness: how many times an arm winds round between the middle and the edge (0 = straight spokes). Width: how much of the space between arms each fills. Direction: which way it turns. Speed: turns a second (per beat on a speed master). The box is treated as if it were round, so the arms reach every corner. The accent is each arm's leading edge. Scope: round the middle of each box, or of the whole canvas; Each box, staggered: each box's spiral a little behind the last (Stagger order; Stagger: how far, as a fraction of a turn).";

    effect.parameters = [
        { name: "speed",     description: "Turns a second (per beat on a speed master)", min: 0.05, max: 3, defaultValue: 0.3 },
        { name: "arms",      description: "How many arms", min: 1, max: 6, defaultValue: 2 },
        { name: "tightness", description: "Times an arm winds round, middle to edge", min: 0, max: 4, defaultValue: 1 },
        { name: "width",     description: "How much of the space between arms each fills", min: 0.1, max: 0.9, defaultValue: 0.5 },
        { name: "direction", description: "Which way it turns", defaultValue: 0, values: ["Clockwise", "Anticlockwise"] },
        { name: "scope",     description: "Round the middle of each box, or of the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.3)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var arms = Math.max(1, Math.round(num(params.arms, 2)));
            var tight = Math.max(0, num(params.tightness, 1));
            var width = MATRIX.clamp(num(params.width, 0.5), 0.05, 0.95);
            var sign = Math.round(num(params.direction, 0)) === 0 ? 1 : -1;
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 140 }, { r: 0, g: 170, b: 255 }]);
            var turn = num(inputs._time, 0) * num(params.speed, 0.3);

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var dx = p.w > 1 ? p.x / (p.w - 1) * 2 - 1 : 0;
                var dy = p.h > 1 ? p.y / (p.h - 1) * 2 - 1 : 0;
                var r = Math.sqrt(dx * dx + dy * dy) / Math.SQRT2;          // 0..1
                var a = Math.atan2(dy, dx) / (2 * Math.PI);                 // turns
                // Where this pixel is in the run of arms, turning with time.
                var s = arms * (sign * a - tight * r - turn);
                var arm = Math.floor(s), fr = s - arm;
                // A pixel's worth of softness across the arm, so it glides.
                var px = Math.max(1, Math.min(p.w, p.h) - 1) / 2;
                var soft = MATRIX.clamp(arms * (tight / px + 1 / (2 * Math.PI * Math.max(0.5, r * px * Math.SQRT2))), 0.04, 0.35);
                var d = Math.abs(fr - 0.5);
                var lvl = MATRIX.clamp((width / 2 - d) / soft + 0.5, 0, 1);
                var acc = fr > 0.5 ? lvl * MATRIX.clamp(1 - (0.5 + width / 2 - fr) / (soft * 1.5), 0, 1) : 0;
                if (r < 0.02) { lvl = 1; acc = 0.4; }                        // the hub
                return MATRIX.out(f, cv, MATRIX.pick(C, arm), lvl, acc * 0.8, params);
            });
        });
    };

    return effect;
})()
