/*
  QLC+ Effect Script: Sparkle
  Fixtures twinkle — each one fires briefly then fades.
  Good for starfield, disco, or ambient background looks.

  Pure & testable: triggers are a hash of the fixture index and the current
  time slice (no Math.random, no wall-clock), so the twinkle is reproducible
  and the script runs identically in the harness and in the app.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Sparkle";
    effect.description = "Random fixtures flash and fade like twinkle stars";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "rgb"];
    effect.notes = "Fixtures flash and fade like stars twinkling or glitter catching light. Density controls how many fixtures light at once; decay sets how quickly each flash fades. Works on dimmers for white sparkle, or RGB wash for coloured pixel-mapping effects.";

    effect.inputs = [];

    // Sparkle colour comes from the LOOK's first Colour palette.

    effect.parameters = [
        { name: "density",  description: "Fraction of fixtures lit at once (0-1)", min: 0.0, max: 1.0, defaultValue: 0.25 },
        { name: "speed",    description: "Flash rate (events/sec per fixture)",     min: 0.1, max: 5.0, defaultValue: 1.0  },
        { name: "dimmer",   description: "Peak dimmer level",                       min: 0.0, max: 1.0, defaultValue: 1.0  },
        { name: "decay",    description: "Fade speed (larger = faster fade)",       min: 0.5, max: 10.0, defaultValue: 3.0  }
    ];

    // Deterministic hash → 0..1 from integer keys (replaces Math.random).
    function hash(a, b) {
        var s = Math.sin(a * 127.1 + b * 311.7) * 43758.5453;
        return s - Math.floor(s);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t       = inputs._time !== undefined ? inputs._time : 0;
        var dt      = 0.02;  // ~50 Hz tick
        var density = params.density !== undefined ? params.density : 0.25;
        var speed   = params.speed   !== undefined ? params.speed   : 1.0;
        var dimmer  = params.dimmer  !== undefined ? params.dimmer  : 1.0;
        var decay   = params.decay   !== undefined ? params.decay   : 3.0;

        // Initialise per-fixture brightness array in state
        if (!state.bright) {
            state.bright = [];
            for (var i = 0; i < fixtures.length; i++)
                state.bright[i] = 0.0;
        }

        // Quantise time into ~50 Hz slices so each tick draws a fresh, stable
        // hash per fixture — same expected trigger probability as the old
        // Math.random() < speed*density*dt, but reproducible.
        var slice = Math.floor(t / dt);

        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var fc = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 };

        return fixtures.map(function(f, i) {
            // Chance of triggering a new flash this tick (deterministic)
            if (hash(i + 1, slice) < speed * density * dt)
                state.bright[i] = 1.0;

            // Exponential decay
            state.bright[i] = Math.max(0, state.bright[i] - decay * dt);

            var b = state.bright[i] * dimmer;
            var intent = {};
            if (f.hasDimmer) intent.dimmer = b;
            intent.r = Math.round(fc.r * b);
            intent.g = Math.round(fc.g * b);
            intent.b = Math.round(fc.b * b);
            return intent;
        });
    };

    return effect;
})()
