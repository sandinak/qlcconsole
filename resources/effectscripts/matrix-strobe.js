/*
  QLC+ Effect Script: Matrix Strobe
  Strobe hits on the beat across the matrix: the whole canvas, alternate
  boxes, a random box, random blocks of pixels, one half then the other,
  or row by row -- in white, in the look's colours, or both.

  Pure & testable: driven by the beat clock / inputs._time; random choices
  from a hash, not Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Strobe";
    effect.description = "Strobe hits on the beat: the whole matrix, boxes, blocks or rows";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A flash on every step: on the beat (Division: how often) or free at Speed flashes a second. Pattern: the whole canvas, alternate boxes, a random box, random blocks of pixels, the left half then the right, or a row at a time. Colour: white (the white pixels and white on the RGB ones), the look's colours (one a flash), or the look's colours with the white pixels hitting white over them. Flash length: how much of each step it stays on; Decay: a hard cut or a fade.";

    effect.parameters = [
        { name: "timing",   description: "Flash on the beat, or free at Speed", defaultValue: 0, values: MATRIX.TIMINGS },
        { name: "division", description: "On the beat: how often it flashes", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Free: flashes a second (per beat on a speed master)", min: 0.5, max: 20, defaultValue: 6 },
        { name: "pattern",  description: "What flashes", defaultValue: 0,
          values: ["Whole canvas", "Alternate boxes", "Random box", "Random blocks", "Halves", "Rows"] },
        { name: "colour",   description: "Flash colour", defaultValue: 0,
          values: ["White", "Look colours", "Look colours + white pixels"] },
        { name: "length",   description: "How much of each step the flash lasts", min: 0.05, max: 0.9, defaultValue: 0.25 },
        { name: "decay",    description: "Hard cut (0) or fade (1)", min: 0, max: 1, defaultValue: 0.2 }
    ].concat(MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var onBeat = Math.round(num(params.timing, 0)) === 0;
        var clk = MATRIX.clock(inputs, num(params.speed, 6), onBeat, MATRIX.perBeat(params.division));
        var step = Math.floor(clk.pos), fr = clk.pos - step;
        var pattern = Math.round(num(params.pattern, 0));
        var colourMode = Math.round(num(params.colour, 0));
        var length = MATRIX.clamp(num(params.length, 0.25), 0.01, 1);
        var decay = MATRIX.clamp(num(params.decay, 0.2), 0, 1);
        var C = MATRIX.colours(palettes, [{ r: 255, g: 255, b: 255 }]);
        var white = { r: 255, g: 255, b: 255 };

        // The flash's level this moment: on for "length" of the step, the
        // last "decay" of that fading.
        var level = 0;
        if (fr < length) {
            var hold = length * (1 - decay);
            level = fr <= hold ? 1 : 1 - (fr - hold) / Math.max(0.0001, length - hold);
        }
        var colour = colourMode === 0 ? white : MATRIX.pick(C, step);
        var units = Math.max(1, cv.count);
        var pickBox = Math.floor(MATRIX.hash(step, 41) * units);

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var b = cv.box[f.id];
            var on = true;
            switch (pattern) {
                case 1: on = b ? ((b.index + step) & 1) === 0 : true; break;
                case 2: on = b ? b.index === pickBox : true; break;
                case 3: {
                    // Blocks of 3 x 2 pixels, about a third of them a flash.
                    var bx = Math.floor(g.col / 3), by = Math.floor(g.row / 2);
                    on = MATRIX.hash(bx * 17 + by * 131, step, 43) < 0.35;
                    break;
                }
                case 4: on = ((g.col < cv.cols / 2) ? 0 : 1) === (step & 1); break;
                case 5: on = cv.rows > 1 ? (g.row === ((step % cv.rows) + cv.rows) % cv.rows) : true; break;
                default: on = true;
            }
            var l = on ? level : 0;
            var acc = (on && (colourMode === 0 || (colourMode === 2 && cv.anyWhite))) ? level : 0;
            return MATRIX.out(f, cv, colour, l, acc, params);
        });
    };

    return effect;
})()
