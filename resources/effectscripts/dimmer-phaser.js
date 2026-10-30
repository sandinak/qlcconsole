/*
  QLC+ Effect Script: Dimmer Phaser
  The MA3/Hog intensity-effect workhorse, as one configurable script. A single
  waveform (sine / saw / triangle / square / random) drives the dimmer, with a
  per-fixture phase spread that turns it into a chase across the group:
    - sine, spread 0          -> a unison Breathe
    - saw down, spread 360    -> a Ramp Chase comet
    - square, low width       -> a bump / strobe-style chase
    - random                  -> a sparkle/twinkle (deterministic per step)
  Intensity-only: drives dimmer (and tints a bound Color), never pan/tilt.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Dimmer Phaser";
    // Takes the host's spread (fixture.phase, inputs._spread) in place of its own.
    effect.hostSpread = "spread,reverse";
    // A wave: rate is its cycles. Run Once (or N times) it plays whole
    // cycles and ends where its wave does -- a sine Breathe on its floor.
    effect.cycleParam = "rate";
    effect.description = "Waveform intensity FX with phase-spread chase";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "rgb", "moving"];
    effect.notes = "A configurable intensity engine. Pick a waveform; rate sets cycles "
                 + "per second; min/max set the dimmer floor and ceiling. spread is the "
                 + "phase offset across the group in degrees — 0 pulses every fixture "
                 + "together, 360 walks one full wave across the array (a chase). width is "
                 + "the on-fraction for the Square waveform (a narrow width gives short "
                 + "bumps). reverse flips the chase direction. Random holds a fresh level "
                 + "per fixture each cycle (a deterministic twinkle).";

    effect.inputs = [
        { name: "level", description: "Master level scaler (0..1)", defaultValue: 1.0 }
    ];

    effect.parameters = [
        { name: "waveform", description: "Waveform", defaultValue: 0,
          values: ["Sine", "Saw Up", "Saw Down", "Triangle", "Square", "Random"] },
        { name: "rate",   description: "Cycles per second",            min: 0.0, max: 20.0, defaultValue: 1.0 },
        { name: "minDim", description: "Minimum dimmer level (0-1)",   min: 0.0, max: 1.0,  defaultValue: 0.0 },
        { name: "maxDim", description: "Maximum dimmer level (0-1)",   min: 0.0, max: 1.0,  defaultValue: 1.0 },
        { name: "spread", description: "Per-fixture phase spread (deg)", min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "width",  description: "Square on-fraction (0.05-0.95)", min: 0.05, max: 0.95, defaultValue: 0.5 },
        { name: "reverse", description: "Reverse chase direction",     min: 0.0, max: 1.0, defaultValue: 0.0 }
    ];

    // Deterministic 0..1 hash (no Math.random — keeps the script pure/testable).
    function hash(n) {
        var x = Math.sin(n * 12.9898) * 43758.5453;
        return x - Math.floor(x);
    }

    // Waveform shaper: phaseFull is in cycles (its fractional part is one period).
    function shape(type, phaseFull, fixIndex, width) {
        var p = phaseFull - Math.floor(phaseFull); // 0..1
        switch (type) {
            case 1: return p;                              // Saw Up
            case 2: return 1.0 - p;                        // Saw Down
            case 3: return p < 0.5 ? 2 * p : 2 * (1 - p);  // Triangle
            case 4: return p < width ? 1.0 : 0.0;          // Square
            case 5: return hash(Math.floor(phaseFull) + fixIndex * 7.13); // Random per step
            default: return 0.5 - 0.5 * Math.cos(2 * Math.PI * p); // Sine (starts at 0)
        }
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t     = inputs._time !== undefined ? inputs._time : 0;
        var level = inputs.level !== undefined ? inputs.level : 1.0;

        var type   = params.waveform !== undefined ? Math.round(params.waveform) : 0;
        var rate   = params.rate     !== undefined ? params.rate     : 1.0;
        var mn      = params.minDim   !== undefined ? params.minDim   : 0.0;
        var mx      = params.maxDim   !== undefined ? params.maxDim   : 1.0;
        var spread = (params.spread  !== undefined ? params.spread  : 0.0) / 360.0;
        var width  = params.width    !== undefined ? params.width    : 0.5;
        var rev    = params.reverse  !== undefined ? params.reverse  : 0.0;

        var n   = Math.max(fixtures.length - 1, 1);
        var dir = (rev > 0.5) ? -1.0 : 1.0;

        return fixtures.map(function(f, i) {
            var phaseFull = t * rate + (inputs._spread ? f.phase : dir * (spread * i / n));
            var w = shape(type, phaseFull, i, width);
            var dimmer = (mn + w * (mx - mn)) * level;

            // The dimmer alone: the host scales the look under it by it --
            // its dimmer, or on a head with none its colour, white and all.
            // Sending the look's colour too dimmed a head twice and blacked
            // its white. (It still names palettes.look, here: that keeps the
            // look's Colour and Dimmer the base it breathes --
            // QLCPalette::effectUsesLookColours.)
            return { dimmer: dimmer };
        });
    };

    return effect;
})()
