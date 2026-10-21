/*
  QLC+ Effect Script: Matrix Chevrons
  Arrows marching across a pixel matrix: > > > to the right, < < < to the
  left, up or down, or out from (in to) the middle -- the festival box
  rig's "this way" chase.

  Pure & testable: driven by inputs._time; no Math.random / wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Chevrons";
    effect.description = "Chevrons (arrows) march across the matrix";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Arrow shapes, each in the next of the look's colours, march the way they point. Direction: right, left, up, down, out from the middle or in to it. Spacing: pixels from one chevron to the next; Thickness: how much of that each one fills; Angle: how sharp the point is. Scope: the middle of the whole canvas, or of each box. Speed: chevrons passing a point each second (per beat on a speed master). The accent is each chevron's leading edge. Each box, staggered: the same in every box, each a little behind the last, so it rolls across the rig -- Stagger order: left to right, right to left, out from the middle or random; Stagger: how far behind, as a fraction of a chevron.";

    effect.parameters = [
        { name: "speed",     description: "Chevrons a second (per beat on a speed master)", min: 0.1, max: 8, defaultValue: 1.5 },
        { name: "direction", description: "Which way they point and go", defaultValue: 0,
          values: ["Right", "Left", "Up", "Down", "Out from the middle", "In to the middle"] },
        { name: "spacing",   description: "Pixels from one chevron to the next", min: 3, max: 30, defaultValue: 6 },
        { name: "thickness", description: "How much of the spacing each fills", min: 0.1, max: 0.9, defaultValue: 0.4 },
        { name: "angle",     description: "Sharpness of the point (pixels across per pixel up)", min: 0, max: 3, defaultValue: 1 },
        { name: "scope",     description: "The middle of the whole canvas, or of each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 1.5)),
                             function(fixtures, inputs, state, root) {
            var num = MATRIX.num;
            var dir = Math.round(num(params.direction, 0));
            var spacing = Math.max(1, num(params.spacing, 6));
            var thick = MATRIX.clamp(num(params.thickness, 0.4), 0.05, 0.95);
            var slope = Math.max(0, num(params.angle, 1));
            var perBox = MATRIX.perBox(params, 0);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 160 }, { r: 0, g: 200, b: 255 }]);
            var phase = num(inputs._time, 0) * num(params.speed, 1.5);

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var cx = (p.w - 1) / 2, cy = (p.h - 1) / 2;
                var along, off;
                if (dir === 0)      { along = p.x;  off = Math.abs(p.y - cy); }
                else if (dir === 1) { along = -p.x; off = Math.abs(p.y - cy); }
                else if (dir === 2) { along = -p.y; off = Math.abs(p.x - cx); }
                else if (dir === 3) { along = p.y;  off = Math.abs(p.x - cx); }
                else if (dir === 4) { along = Math.abs(p.x - cx);  off = Math.abs(p.y - cy); }
                else                { along = -Math.abs(p.x - cx); off = Math.abs(p.y - cy); }
                // A chevron is a line of equal (along + off * slope): its point
                // leads. Its place in the run, going forward with time:
                var s = (along + off * slope) / spacing - phase;
                var idx = Math.floor(s);
                var fr = s - idx;                       // 0 = the chevron's front edge
                var front = 1 - fr;                     // 1 at the front edge, falling behind it
                var lvl = (front <= thick) ? 1 : 0;
                // A soft pixel's width at the back edge, so it doesn't step.
                var px = 1 / spacing;
                if (!lvl && front < thick + px) lvl = 1 - (front - thick) / px;
                var acc = (lvl > 0 && front < px * 1.2) ? 1 - front / (px * 1.2) : 0;
                return MATRIX.out(f, cv, MATRIX.pick(C, -idx), lvl, acc, params);
            });
        });
    };

    return effect;
})()
