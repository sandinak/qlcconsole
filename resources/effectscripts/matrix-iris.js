/*
  QLC+ Effect Script: Matrix Iris
  An eye that opens and closes: a lit shape grows out of the middle of each
  box (or the whole canvas) and shrinks back -- breathing, blinking, or
  thrown open on every beat.

  Pure & testable: driven by inputs._time / the beat clock; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Iris";
    effect.description = "An eye opens and closes in the middle of each box";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A shape opens out of the middle and closes again, filled with the look's first colour; its rim takes the second colour, if there is one. Shape: an eye (lids meeting at the corners), a circle, a box, or a vertical slit. Motion: breathe (smoothly open and closed), blink (open, with quick blinks), or open on the beat (snaps open and closes over the step; Division: how often). Edge: how soft the rim is; Background: a glow of the second colour outside. Scope: in each box, or one across the whole canvas. The accent is the rim. Each box, staggered: the same in every box, each a little behind the last, so it rolls across the rig -- Stagger order: left to right, right to left, out from the middle or random; Stagger: how far behind, as a fraction of a open-and-close.";

    effect.parameters = [
        { name: "motion",     description: "How it opens and closes", defaultValue: 0, values: ["Breathe", "Blink", "Open on the beat"] },
        { name: "division",   description: "On the beat: how often it opens", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",      description: "Cycles a second (per beat on a speed master)", min: 0.05, max: 4, defaultValue: 0.4 },
        { name: "shape",      description: "The opening's shape", defaultValue: 0, values: ["Eye", "Circle", "Box", "Vertical slit"] },
        { name: "edge",       description: "Rim softness", min: 0, max: 1, defaultValue: 0.3 },
        { name: "background", description: "A glow of the second colour outside", min: 0, max: 0.5, defaultValue: 0 },
        { name: "scope",      description: "In each box, or one across the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.4), MATRIX.perBeat(params.division)),
                             function(fixtures, inputs, state, root) {
            var num = MATRIX.num;
            var motion = Math.round(num(params.motion, 0));
            var shape = Math.round(num(params.shape, 0));
            var edge = MATRIX.clamp(num(params.edge, 0.3), 0, 1);
            var bg = MATRIX.clamp(num(params.background, 0), 0, 1);
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 120, b: 0 }, { r: 120, g: 0, b: 255 }]);
            var inner = C[0], rim = C.length > 1 ? C[1] : C[0];
            var clk = MATRIX.clock(inputs, num(params.speed, 0.4), motion === 2, MATRIX.perBeat(params.division));
            var fr = clk.pos - Math.floor(clk.pos);
            var open;
            if (motion === 1) {
                // Open, with a quick blink at the start of each cycle.
                open = fr < 0.12 ? Math.abs(Math.cos(fr / 0.12 * Math.PI)) : 1;
            } else if (motion === 2) {
                open = 1 - fr;
                open = 1 - (1 - open) * (1 - open);                    // snaps open, eases shut
            } else {
                open = 0.5 - 0.5 * Math.cos(fr * 2 * Math.PI);
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                // -1..1 each way from the middle, to the pixels' outer edges (so
                // a box of two rows still has an eye to open).
                var dx = (p.x + 0.5 - p.w / 2) / (p.w / 2), dy = (p.y + 0.5 - p.h / 2) / (p.h / 2);
                var d;                                                 // 0 at the middle, 1 at the opening's widest
                if (shape === 1) d = Math.sqrt(dx * dx + dy * dy) / Math.SQRT2;
                else if (shape === 2) d = Math.max(Math.abs(dx), Math.abs(dy));
                else if (shape === 3) d = Math.abs(dx);
                else {
                    // The eye: lids at +-open*(1-x^2); d is how far up the lid this pixel is.
                    var lid = Math.max(0.0001, 1 - dx * dx);
                    d = Math.abs(dy) / lid;
                    if (Math.abs(dx) >= 1) d = Math.max(d, 1.0001);
                }
                var o = Math.max(0, open) * 1.05;                     // fully open reaches the corners
                var soft = 0.05 + edge * 0.5;
                var lvl, acc = 0, colour = inner;
                if (d <= o - soft) lvl = 1;
                else if (d <= o) {                                     // the rim
                    lvl = 1;
                    colour = MATRIX.mix(inner, rim, (d - (o - soft)) / soft);
                    acc = 1 - (o - d) / soft;
                } else if (d <= o + soft * 0.5 && o > 0.02) {
                    lvl = 1 - (d - o) / (soft * 0.5);
                    colour = rim;
                } else lvl = 0;
                if (lvl < bg) { lvl = bg; colour = rim; }
                if (o <= 0.02 && d > 0) lvl = Math.min(lvl, bg);       // shut
                return MATRIX.out(f, cv, colour, lvl, acc * (open > 0.05 ? 1 : 0), params);
            });
        });
    };

    return effect;
})()
