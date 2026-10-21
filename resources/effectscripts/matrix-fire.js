/*
  QLC+ Effect Script: Matrix Fire
  Flames licking up from the bottom of the canvas (or of each box): a bed
  of heat that flickers, tongues that rise and break off, white-hot at the
  base. Made for a few big pixels -- five rows is plenty of fire.

  Pure & testable: driven by inputs._time; turbulence from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Fire";
    effect.description = "Flames rise from the bottom and flicker";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Flames rise from the bottom and flicker. Height: how far up the flames reach (1 = the top row). Intensity: how hot the fire burns -- low for embers, high for a blaze. Cooling: how quickly the tongues die away as they rise (high = ragged, broken tops). Speed: how fast it flickers and rises (per beat on a speed master). Palette: the look's colours from cool to hot (classic fire when the look has none), classic fire, or blue fire. The accent is the white-hot heart of the flames. Scope: one fire across the whole canvas, or a fire in each box; Each box, staggered: each box's fire a moment behind the last (Stagger order; Stagger: how far, as a fraction of a second at Speed 1).";

    effect.parameters = [
        { name: "speed",     description: "How fast it flickers and rises (per beat on a speed master)", min: 0.1, max: 4, defaultValue: 1 },
        { name: "height",    description: "How far up the flames reach (1 = the top)", min: 0.2, max: 1.5, defaultValue: 1 },
        { name: "intensity", description: "How hot it burns", min: 0.1, max: 1, defaultValue: 0.8 },
        { name: "cooling",   description: "How quickly the tongues die away", min: 0, max: 1, defaultValue: 0.45 },
        { name: "palette",   description: "Colouring", defaultValue: 0, values: ["Look colours (fire without)", "Classic fire", "Blue fire"] },
        { name: "scope",     description: "One fire across the canvas, or one in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    var CLASSIC = [{ r: 110, g: 0, b: 0 }, { r: 220, g: 25, b: 0 }, { r: 255, g: 90, b: 0 },
                   { r: 255, g: 165, b: 10 }, { r: 255, g: 225, b: 90 }];
    var BLUE    = [{ r: 0, g: 0, b: 110 }, { r: 0, g: 40, b: 230 }, { r: 0, g: 130, b: 255 },
                   { r: 80, g: 200, b: 255 }, { r: 200, g: 240, b: 255 }];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 1)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var tt = num(inputs._time, 0) * num(params.speed, 1);
            var height = Math.max(0.05, num(params.height, 1));
            var heat0 = MATRIX.clamp(num(params.intensity, 0.8), 0, 1);
            var cooling = MATRIX.clamp(num(params.cooling, 0.45), 0, 1);
            var pal = Math.round(num(params.palette, 0));
            var perBox = MATRIX.perBox(params, 0);
            var stops = pal === 2 ? BLUE : (pal === 1 ? CLASSIC : MATRIX.colours(palettes, CLASSIC));

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var rows = Math.max(1, p.h);
                var y = p.h - 1 - p.y;                               // rows up from the bottom
                var up = (y + 0.5) / rows;                           // 0..1, bottom to top
                // Each column's flames reach a height of their own that comes and goes.
                var reach = height * (0.5 + 1.0 * MATRIX.noise(tt * 2.3, p.x * 0.37, 29));
                // Turbulence rising through the flames.
                var sway = 0.6 * Math.sin(tt * 1.3 + y * 0.9);
                var n = 0.62 * MATRIX.noise(p.x * 0.42 + sway, y * 0.55 - tt * 2.4, 3)
                      + 0.38 * MATRIX.noise(p.x * 0.95 + 7.1, y * 1.1 - tt * 3.9, 11);
                var base = 1 - up / Math.max(0.05, reach);
                var heat = base > 0 ? Math.pow(base, 0.3 + cooling * 1.2) : 0;
                heat *= (0.15 + 1.45 * n) * (0.4 + 0.8 * heat0);
                // The bed of embers along the bottom row never quite goes out.
                if (y === 0) heat = Math.max(heat, 0.25 * heat0 * (0.6 + 0.8 * n));
                heat = MATRIX.clamp(heat, 0, 1);
                var colour = MATRIX.ramp(stops, heat, false);
                var lvl = MATRIX.clamp(heat * 1.7, 0, 1);
                var acc = 0.8 * MATRIX.smooth(0.78, 1, heat);
                return MATRIX.out(f, cv, colour, lvl, acc, params);
            });
        });
    };

    return effect;
})()
