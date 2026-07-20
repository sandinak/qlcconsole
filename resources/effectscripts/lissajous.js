/*
  QLC+ Effect Script: Lissajous
  Full Lissajous curve on each fixture with INDEPENDENT pan/tilt frequency and
  phase — the general case of the classic EFX "Lissajous" algorithm (Circle,
  Figure-8, and every knot in between are special cases). Bind a Colour/Dimmer
  look for looks; this drives pan/tilt only and composes on top.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Lissajous";
    effect.description  = "General Lissajous pan/tilt curve (arbitrary X/Y freq + phase)";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "The general Lissajous figure: pan = sin(xFreq·θ + xPhase), tilt = sin(yFreq·θ + yPhase). xFreq=yFreq=1 with a 90° phase difference gives a circle; 2:1 gives a figure-8; equal integer ratios give closed knots. This is the full-parameter version of the stock EFX Lissajous algorithm — keep EFX for legacy shows; use this to compose Lissajous movement on top of Colour/Dimmer looks. Phase spread staggers fixtures into a ribbon. Supports beat sync.";

    effect.inputs = [
        { name: "x", description: "Figure centre pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Figure centre tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    effect.parameters = [
        { name: "speed",       description: "Cycles per second",                    min: -3.0, max: 3.0, defaultValue: 0.3 },
        { name: "panRadius",   description: "Pan half-width (fraction of range)",    min: 0.0,  max: 0.5, defaultValue: 0.25 },
        { name: "tiltRadius",  description: "Tilt half-width (fraction of range)",   min: 0.0,  max: 0.5, defaultValue: 0.2 },
        { name: "xFreq",       description: "Pan frequency (integer for closed curves)",  min: 1.0, max: 8.0, defaultValue: 1.0 },
        { name: "yFreq",       description: "Tilt frequency (integer for closed curves)", min: 1.0, max: 8.0, defaultValue: 2.0 },
        { name: "xPhase",      description: "Pan phase (degrees)",                   min: 0.0, max: 360.0, defaultValue: 90.0 },
        { name: "yPhase",      description: "Tilt phase (degrees)",                  min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "phaseSpread", description: "Phase spread across fixtures (0=clump, 1=full cycle)", min: 0.0, max: 1.0, defaultValue: 0.8 },
        { name: "beatSync",    description: "Figures per beat (0=off, 1=1/beat)",    min: 0.0, max: 4.0, defaultValue: 0.0 }
    ];

    var DEG = Math.PI / 180.0;

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t         = inputs._time      !== undefined ? inputs._time      : 0;
        var beat      = inputs._beat      !== undefined ? inputs._beat      : 0;
        var bpm       = inputs._bpm       !== undefined ? inputs._bpm       : 0;
        var beatCount = inputs._beatCount !== undefined ? inputs._beatCount : 0;
        var cx        = inputs.x          !== undefined ? inputs.x          : 0.5;
        var cy        = inputs.y          !== undefined ? inputs.y          : 0.5;

        var speed       = params.speed       !== undefined ? params.speed       : 0.3;
        var panRadius   = params.panRadius   !== undefined ? params.panRadius   : 0.25;
        var tiltRadius  = params.tiltRadius  !== undefined ? params.tiltRadius  : 0.2;
        var xFreq       = params.xFreq       !== undefined ? params.xFreq       : 1.0;
        var yFreq       = params.yFreq       !== undefined ? params.yFreq       : 2.0;
        var xPhase      = (params.xPhase     !== undefined ? params.xPhase      : 90.0) * DEG;
        var yPhase      = (params.yPhase     !== undefined ? params.yPhase      : 0.0)  * DEG;
        var phaseSpread = params.phaseSpread !== undefined ? params.phaseSpread : 0.8;
        var beatSync    = params.beatSync    !== undefined ? params.beatSync    : 0.0;

        var n = Math.max(fixtures.length, 1);

        var baseTheta;
        if (beatSync > 0 && bpm > 0)
            baseTheta = (beatCount + beat) * beatSync * 2 * Math.PI;
        else
            baseTheta = t * speed * 2 * Math.PI;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            var fixturePhase = (n > 1) ? (i / (n - 1)) * phaseSpread * 2 * Math.PI : 0;
            var theta = baseTheta - fixturePhase;

            var pan  = (cx + panRadius  * Math.sin(xFreq * theta + xPhase)) * f.panRange;
            var tilt = (cy + tiltRadius * Math.sin(yFreq * theta + yPhase)) * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                intent.dimmer = palettes.dimmer.dimmer;
            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = palettes.color.r; intent.g = palettes.color.g; intent.b = palettes.color.b;
            }
            return intent;
        });
    };

    return effect;
})()
