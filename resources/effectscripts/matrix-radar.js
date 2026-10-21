/*
  QLC+ Effect Script: Matrix Radar
  A line turns round the middle of each box (or the whole canvas) like a
  radar sweep, a fading wake behind it -- or two, three, four blades
  turning together like a fan.

  Pure & testable: driven by inputs._time; no Math.random / wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Radar";
    effect.description = "A line sweeps round the middle like radar, a wake fading behind it";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "One or more lines turn round the middle of each box (or of the whole canvas), each in the next of the look's colours, with a wake fading behind. Speed: turns a second (per beat on a speed master). Blades: lines turning together. Width: each line's width (degrees); Wake: how far behind it fades (degrees). The box is swept as if it were round, so the line takes as long over its short sides as its long ones. The accent is the line itself. Each box, staggered: the same in every box, each a little behind the last, so it rolls across the rig -- Stagger order: left to right, right to left, out from the middle or random; Stagger: how far behind, as a fraction of a turn.";

    effect.parameters = [
        { name: "speed",     description: "Turns a second (per beat on a speed master)", min: 0.05, max: 3, defaultValue: 0.5 },
        { name: "blades",    description: "Lines turning together", min: 1, max: 4, defaultValue: 1 },
        { name: "width",     description: "Line width (degrees)", min: 2, max: 90, defaultValue: 14 },
        { name: "wake",      description: "Wake behind the line (degrees)", min: 0, max: 330, defaultValue: 120 },
        { name: "direction", description: "Which way it turns", defaultValue: 0, values: ["Clockwise", "Anticlockwise"] },
        { name: "scope",     description: "Round the middle of each box, or of the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.5)),
                             function(fixtures, inputs, state, root) {
            var num = MATRIX.num;
            var blades = Math.max(1, Math.round(num(params.blades, 1)));
            var width = MATRIX.clamp(num(params.width, 14), 1, 180) / 360;   // in turns
            var wake = Math.max(0, num(params.wake, 120)) / 360;
            var cw = Math.round(num(params.direction, 0)) === 0;
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 0, g: 255, b: 80 }, { r: 255, g: 60, b: 0 }]);
            var turn = num(inputs._time, 0) * num(params.speed, 0.5);
            var sector = 1 / blades;

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                // As if the box were round: -1..1 each way.
                var dx = p.w > 1 ? p.x / (p.w - 1) * 2 - 1 : 0;
                var dy = p.h > 1 ? p.y / (p.h - 1) * 2 - 1 : 0;
                var a = Math.atan2(dy, dx) / (2 * Math.PI);          // turns, screen clockwise (y down)
                if (!cw) a = -a;
                var behind = turn - a;                                // how far the line is past this pixel
                behind = behind - Math.floor(behind);
                var k = Math.floor(behind / sector);                  // which blade has just passed
                behind -= k * sector;
                var lvl = 0, acc = 0;
                if (behind < width) { lvl = 1; acc = 1 - behind / width; }
                else if (wake > 0 && behind < width + wake) {
                    lvl = 1 - (behind - width) / wake;
                    lvl *= lvl;
                }
                if (dx === 0 && dy === 0) { lvl = 1; acc = 0.5; }     // the hub
                return MATRIX.out(f, cv, MATRIX.pick(C, k), lvl, acc, params);
            });
        });
    };

    return effect;
})()
