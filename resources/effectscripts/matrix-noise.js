/*
  QLC+ Effect Script: Matrix Noise
  Slow-drifting clouds of colour across the matrix -- plasma made for a few
  big pixels rather than a screen: broad shapes that read at 15 x 5, with
  white glints where the field peaks.

  Pure & testable: driven by inputs._time; noise from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Noise";
    effect.description = "Drifting clouds of colour, made for big low-res pixels";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    // The host's spread: each head's phase moves it along in time.
    effect.hostSpread  = "";
    effect.notes = "A smooth field of noise drifts across the pixels and is coloured from a palette: the look's colours (blended), a rainbow, fire or ocean. Scale: the size of the clouds (bigger = broader shapes); Speed: how fast they change (per beat on a speed master); Drift: how fast they slide across; Contrast: soft and even, or punchy with dark gaps; Glints: white glints where the field peaks (the accent). Spread (the look's Spread row) puts each head further along in time.";

    effect.parameters = [
        { name: "speed",    description: "How fast it changes (per beat on a speed master)", min: 0.02, max: 3, defaultValue: 0.3 },
        { name: "scale",    description: "Cloud size (pixels)", min: 1, max: 30, defaultValue: 6 },
        { name: "drift",    description: "Slide across (pixels a second)", min: -10, max: 10, defaultValue: 1.5 },
        { name: "contrast", description: "Soft and even, or punchy with dark gaps", min: 0, max: 1, defaultValue: 0.4 },
        { name: "palette",  description: "Colouring", defaultValue: 0, values: ["Look colours", "Rainbow", "Fire", "Ocean"] },
        { name: "glints",   description: "White glints where the field peaks", min: 0, max: 0.3, defaultValue: 0.06 }
    ].concat(MATRIX.whiteParams(0));

    var FIRE  = [{ r: 0, g: 0, b: 0 }, { r: 180, g: 0, b: 0 }, { r: 255, g: 90, b: 0 }, { r: 255, g: 200, b: 40 }];
    var OCEAN = [{ r: 0, g: 10, b: 60 }, { r: 0, g: 80, b: 200 }, { r: 0, g: 200, b: 220 }, { r: 120, g: 255, b: 230 }];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var t = num(inputs._time, 0);
        var speed = num(params.speed, 0.3);
        var scale = Math.max(0.5, num(params.scale, 6));
        var drift = num(params.drift, 1.5);
        var contrast = MATRIX.clamp(num(params.contrast, 0.4), 0, 1);
        var pal = Math.round(num(params.palette, 0));
        var glints = MATRIX.clamp(num(params.glints, 0.06), 0, 1);
        var host = !!inputs._spread;
        var L = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 120 }, { r: 60, g: 0, b: 255 }, { r: 0, g: 200, b: 255 }]);

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var z = (t + (host ? num(f.phase, 0) : 0)) * speed;
            var x = (g.col - t * drift) / scale, y = g.row / scale;
            // Two octaves, blended between two moments so it changes smoothly.
            var zi = Math.floor(z), zf = z - zi;
            zf = zf * zf * (3 - 2 * zf);
            function field(seed) {
                return 0.65 * MATRIX.noise(x, y, seed) + 0.35 * MATRIX.noise(x * 2.1 + 5.3, y * 2.1 + 1.7, seed + 50);
            }
            var v = field(zi) * (1 - zf) + field(zi + 1) * zf;
            v = MATRIX.clamp((v - 0.5) * 1.6 + 0.5, 0, 1);        // use the whole range
            var colour;
            if (pal === 1) colour = MATRIX.hue(v * 1.2 + t * speed * 0.1);
            else if (pal === 2) colour = MATRIX.ramp(FIRE, v, false);
            else if (pal === 3) colour = MATRIX.ramp(OCEAN, v, false);
            else colour = MATRIX.ramp(L, v, L.length > 2);
            // Contrast: below a falling floor the field goes dark.
            var lvl = contrast > 0 ? MATRIX.smooth(contrast * 0.6, contrast * 0.6 + 0.25, v) : 1;
            var acc = glints > 0 ? MATRIX.smooth(1 - glints, 1 - glints * 0.3, v) : 0;
            return MATRIX.out(f, cv, colour, lvl, acc, params);
        });
    };

    return effect;
})()
