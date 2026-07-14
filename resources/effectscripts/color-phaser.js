/*
  QLC+ Effect Script: Color Phaser
  An MA3-style colour effect: a waveform crossfades each fixture between two
  Color palettes over time, with a per-fixture phase spread so the blend
  travels across the group as a wave.
    - spread 0            -> the whole group breathes between the two colours
    - spread 360, sine    -> a smooth colour wave rolls across the array
    - square waveform      -> a hard A/B colour chase
  Colour-only: drives r/g/b (scaled by an optional Dimmer), leaves pan/tilt.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Phaser";
    effect.description = "Waveform crossfade between two colours with phase spread";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];
    effect.notes       = "Colours come from the LOOK: stack two Colour palettes (e.g. Red then "
                       + "Blue) and the effect crossfades between them over time using the chosen "
                       + "waveform. rate is cycles per second; spread is the per-fixture phase "
                       + "offset in degrees, so 360 walks one full colour wave across the "
                       + "group. Square gives a hard A/B chase, Triangle a steady ramp "
                       + "back and forth, Sine a smooth pulse. reverse flips the wave "
                       + "direction. Intensity is left to the scene's Dimmer look.";

    // No palette slots: the two ends come from the look's Colour palettes.
    effect.parameters = [
        { name: "waveform", description: "Waveform", defaultValue: 0,
          values: ["Sine", "Triangle", "Square", "Saw"] },
        { name: "rate",    description: "Cycles per second",            min: 0.0, max: 10.0, defaultValue: 0.5 },
        { name: "spread",  description: "Per-fixture phase spread (deg)", min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "reverse", description: "Reverse wave direction",       min: 0.0, max: 1.0, defaultValue: 0.0 }
    ];

    // Waveform shaper -> 0..1 blend (0 = colour A, 1 = colour B).
    function shape(type, phaseFull) {
        var p = phaseFull - Math.floor(phaseFull); // 0..1
        switch (type) {
            case 1: return p < 0.5 ? 2 * p : 2 * (1 - p);  // Triangle
            case 2: return p < 0.5 ? 0.0 : 1.0;            // Square (hard A/B)
            case 3: return p;                              // Saw
            default: return 0.5 - 0.5 * Math.cos(2 * Math.PI * p); // Sine
        }
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        // Gradient ends = the look's first two Colour palettes (precedence order).
        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var c1 = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 0, b: 0 };
        var c2 = (L[1] && L[1].r !== undefined) ? L[1] : { r: 0, g: 0, b: 255 };

        var t      = inputs._time !== undefined ? inputs._time : 0;
        var type   = params.waveform !== undefined ? Math.round(params.waveform) : 0;
        var rate   = params.rate     !== undefined ? params.rate     : 0.5;
        var spread = (params.spread  !== undefined ? params.spread  : 0.0) / 360.0;
        var rev    = params.reverse  !== undefined ? params.reverse  : 0.0;

        var n   = Math.max(fixtures.length - 1, 1);
        var dir = (rev > 0.5) ? -1.0 : 1.0;

        return fixtures.map(function(f, i) {
            var phaseFull = t * rate + dir * (spread * i / n);
            var blend = shape(type, phaseFull);

            // Colour only — intensity is the scene Dimmer look's job.
            return {
                r: Math.round(c1.r + (c2.r - c1.r) * blend),
                g: Math.round(c1.g + (c2.g - c1.g) * blend),
                b: Math.round(c1.b + (c2.b - c1.b) * blend)
            };
        });
    };

    return effect;
})()
