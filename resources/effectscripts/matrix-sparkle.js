/*
  QLC+ Effect Script: Matrix Sparkle
  Pixels twinkle in the look's colours and fade, while the white pixels
  throw quick bright flashes -- glitter over colour, the box matrix's
  favourite texture.

  Pure & testable: driven by inputs._time; randomness from a hash, not
  Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Sparkle";
    effect.description = "Colour twinkles with quick white flashes on the white pixels";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Pixels light at random in the look's colours and fade away. Density: how many twinkle; Decay: how long each takes to fade (seconds, or beats on a speed master); Flashes: how often quick white flashes go off -- on the white pixels where there are any, on the RGB ones where not; Background: a glow of the look's first colour under it all. The flashes are the accent, so White: Accents only gives colour twinkles with white glitter.";

    effect.parameters = [
        { name: "density",    description: "How many pixels twinkle", min: 0.02, max: 1, defaultValue: 0.25 },
        { name: "decay",      description: "Fade time (seconds; beats on a speed master)", min: 0.05, max: 2, defaultValue: 0.5 },
        { name: "flashes",    description: "How often the white flashes go off", min: 0, max: 1, defaultValue: 0.4 },
        { name: "background", description: "A glow of the first colour under it all", min: 0, max: 0.5, defaultValue: 0 }
    ].concat(MATRIX.whiteParams(0));

    // Each pixel lives in slots of SLOT seconds; in a slot it may spark once,
    // at a moment of its own. Look back far enough to see a spark still fading.
    var SLOT = 0.5;

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var t = num(inputs._time, 0);
        var density = MATRIX.clamp(num(params.density, 0.25), 0, 1);
        var decay = Math.max(0.02, num(params.decay, 0.5));
        var flashes = MATRIX.clamp(num(params.flashes, 0.4), 0, 1);
        var bg = MATRIX.clamp(num(params.background, 0), 0, 1);
        var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 200 }, { r: 0, g: 160, b: 255 }, { r: 255, g: 140, b: 0 }]);
        var slot = Math.floor(t / SLOT);
        var back = Math.ceil(decay / SLOT) + 1;
        var FLASH = 0.09;                                // a flash's length

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var key = g.col * 131 + g.row * 7919 + 1;
            var best = 0, which = 0, acc = 0;
            for (var k = slot; k > slot - back && k >= -1; k--) {
                if (MATRIX.hash(key, k, 1) < density) {
                    var at = (k + MATRIX.hash(key, k, 2)) * SLOT;
                    var age = t - at;
                    if (age >= 0 && age < decay) {
                        var l = 1 - age / decay;
                        l *= l;
                        if (l > best) { best = l; which = Math.floor(MATRIX.hash(key, k, 3) * 997); }
                    }
                }
                if (k === slot || k === slot - 1) {
                    if (MATRIX.hash(key, k, 4) < flashes * 0.12) {
                        var fat = (k + MATRIX.hash(key, k, 5)) * SLOT;
                        var fage = t - fat;
                        if (fage >= 0 && fage < FLASH) acc = Math.max(acc, 1 - fage / FLASH);
                    }
                }
            }
            var colour = MATRIX.pick(C, which);
            var lvl = best;
            if (bg > 0 && best < bg) {
                // The glow, unless a twinkle is brighter.
                colour = MATRIX.mix(C[0], colour, best / bg);
                lvl = bg;
            }
            return MATRIX.out(f, cv, colour, lvl, acc, params);
        });
    };

    return effect;
})()
