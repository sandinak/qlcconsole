/*
  QLC+ Effect Script: Confetti
  Random coloured speckles that pop and fade — the WLED / FastLED "Confetti"
  idea ported to the look system. Each frame a few fixtures flash to a random
  colour (drawn from the LOOK's Colour palettes, or a full rainbow) and are
  then faded toward black, so the rig shimmers like scattered confetti.

  Pure & testable: the randomness is a seeded hash of the fixture/spark index
  and the current time slice (no Math.random, no wall-clock), so the pattern is
  reproducible and identical in the harness and in the app.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Confetti";
    effect.description = "Random coloured speckles that blink and fade";
    effect.author      = "WLED/FastLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.notes = "Colourful confetti: each frame a few fixtures pop to a random colour, then fade. Colours come from the look's stacked Colour palettes (add a couple of colours to the look) or, with the Rainbow option, span the full hue wheel. Density sets how many pop per frame; Fade sets how quickly they decay (higher = shorter trails). Randomness is seeded, so it is reproducible.";

    effect.parameters = [
        { name: "density", description: "Speckles per frame (relative)",          min: 0.0,  max: 1.0, defaultValue: 0.3  },
        { name: "fade",    description: "Fade speed (higher = shorter trails)",    min: 0.02, max: 0.6, defaultValue: 0.12 },
        { name: "colors",  description: "Colour source",                           defaultValue: 0, values: ["Look palette", "Rainbow"] }
    ];

    // Deterministic hash → 0..1 from two integer keys (replaces Math.random).
    function hash(a, b) {
        var s = Math.sin(a * 127.1 + b * 311.7) * 43758.5453;
        return s - Math.floor(s);
    }

    // Minimal HSV(hue 0..1, full sat/val) → {r,g,b} 0..255 for the rainbow mode.
    function hsv(h) {
        var i = Math.floor(h * 6), f = h * 6 - i;
        var q = 1 - f, t = f, r, g, b;
        switch (i % 6) {
            case 0:  r = 1; g = t; b = 0; break;
            case 1:  r = q; g = 1; b = 0; break;
            case 2:  r = 0; g = 1; b = t; break;
            case 3:  r = 0; g = q; b = 1; break;
            case 4:  r = t; g = 0; b = 1; break;
            default: r = 1; g = 0; b = q; break;
        }
        return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n = fixtures.length;
        if (n === 0) return [];

        var t       = inputs._time !== undefined ? inputs._time : 0;
        var dt      = 0.02;                       // ~50 Hz tick
        var slice   = Math.floor(t / dt);         // stable per-tick seed
        var density = params.density !== undefined ? params.density : 0.3;
        var fade    = params.fade    !== undefined ? params.fade    : 0.12;
        var rainbow = ((params.colors | 0) === 1);

        // Per-fixture RGB accumulators (floats), persisted + faded across ticks.
        if (!state.rgb || state.rgb.length !== n) {
            state.rgb = [];
            for (var i = 0; i < n; i++) state.rgb[i] = [0, 0, 0];
        }

        // Fade everything toward black.
        var keep = 1 - fade; if (keep < 0) keep = 0;
        for (var i = 0; i < n; i++) {
            state.rgb[i][0] *= keep;
            state.rgb[i][1] *= keep;
            state.rgb[i][2] *= keep;
        }

        // Colour source: the look's stacked Colour palettes, or a rainbow.
        var L = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var haveLook = !rainbow && L.length > 0;

        // Sprinkle a few speckles this frame (deterministic per time slice), each
        // ADDED to its fixture so overlaps brighten — the confetti "catch light".
        var sparks = Math.round(n * density * 0.5);
        if (sparks < 1) sparks = 1;
        for (var s = 0; s < sparks; s++) {
            var idx   = Math.floor(hash(slice + 1, s * 3 + 1) * n) % n;
            var atten = 0.5 + 0.5 * hash(slice + 2, s * 3 + 2);   // 0.5..1 brightness
            var col;
            if (haveLook) {
                var ci = Math.floor(hash(slice + 3, s * 3 + 3) * L.length) % L.length;
                col = L[ci];
            } else {
                col = hsv(hash(slice + 5, s * 7 + 1));
            }
            var a = state.rgb[idx];
            a[0] = Math.min(255, a[0] + col.r * atten);
            a[1] = Math.min(255, a[1] + col.g * atten);
            a[2] = Math.min(255, a[2] + col.b * atten);
        }

        return fixtures.map(function(f, i) {
            var c = state.rgb[i];
            var intent = { r: Math.round(c[0]), g: Math.round(c[1]), b: Math.round(c[2]) };
            // Drive a master dimmer (if any) from the brightest channel so plain
            // dimmer fixtures still sparkle.
            if (f.hasDimmer) intent.dimmer = Math.max(c[0], c[1], c[2]) / 255;
            return intent;
        });
    };

    return effect;
})()
