/*
  QLC+ Effect Script: Figure-8
  Each fixture traces a Lissajous figure (figure-8, trefoil, or bow-tie)
  using a 2:1 pan:tilt or 3:2 frequency ratio. Phase spread staggers
  fixtures around the curve so they form a ribbon rather than clumping.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Figure-8";
    effect.description  = "Lissajous figure-8 or trefoil path on each fixture";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Each fixture traces a Lissajous curve. The ratio parameter selects the pan:tilt frequency integer ratio: 2:1 gives a figure-8, 3:2 gives a more complex bow-tie knot. Phase spread staggers fixtures along the curve so they look like a ribbon of light. panRadius and tiltRadius scale the shape independently. Supports beat sync: one complete figure per N beats.";

    effect.inputs = [
        { name: "x", description: "Figure center pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Figure center tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true },
        { name: "dimmer", type: "Dimmer", optional: true }
    ];

    effect.parameters = [
        { name: "speed",       description: "Cycles per second",                          min: -3.0, max: 3.0,  defaultValue: 0.3  },
        { name: "panRadius",   description: "Pan half-width (fraction of range)",          min: 0.0,  max: 0.5,  defaultValue: 0.2  },
        { name: "tiltRadius",  description: "Tilt half-width (fraction of range)",         min: 0.0,  max: 0.5,  defaultValue: 0.15 },
        { name: "ratio",       description: "Pan:tilt frequency ratio", min: 0, max: 2, defaultValue: 0,
          values: ["2:1 (figure-8)", "3:2 (trefoil)", "3:1 (3-petal)"] },
        { name: "phaseSpread", description: "Phase spread across fixtures (0=clump, 1=full cycle)", min: 0.0, max: 1.0, defaultValue: 0.8 },
        { name: "beatSync",    description: "Figures per beat (0=off, 1=1/beat, 0.5=1/2beats)", min: 0.0, max: 4.0, defaultValue: 0.0 }
    ];

    // Lissajous: pan = sin(pFreq * θ + δ),  tilt = sin(tFreq * θ)
    // Ratios: [panFreq, tiltFreq, phaseOffset] for classic closed curves
    var RATIOS = [
        [2, 1, Math.PI / 2],   // figure-8: symmetric hourglass
        [3, 2, Math.PI / 4],   // trefoil / bow-tie knot
        [3, 1, Math.PI / 2]    // 3-petal
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t         = inputs._time      !== undefined ? inputs._time      : 0;
        var beat      = inputs._beat      !== undefined ? inputs._beat      : 0;
        var bpm       = inputs._bpm       !== undefined ? inputs._bpm       : 0;
        var beatCount = inputs._beatCount !== undefined ? inputs._beatCount : 0;
        var cx        = inputs.x          !== undefined ? inputs.x          : 0.5;
        var cy        = inputs.y          !== undefined ? inputs.y          : 0.5;

        var speed       = params.speed       !== undefined ? params.speed       : 0.3;
        var panRadius   = params.panRadius   !== undefined ? params.panRadius   : 0.2;
        var tiltRadius  = params.tiltRadius  !== undefined ? params.tiltRadius  : 0.15;
        var ratioIdx    = params.ratio       !== undefined ? Math.round(params.ratio) : 0;
        var phaseSpread = params.phaseSpread !== undefined ? params.phaseSpread : 0.8;
        var beatSync    = params.beatSync    !== undefined ? params.beatSync    : 0.0;

        var r = RATIOS[Math.max(0, Math.min(2, ratioIdx))];
        var pFreq = r[0], tFreq = r[1], delta = r[2];

        var n = Math.max(fixtures.length, 1);

        var baseTheta;
        if (beatSync > 0 && bpm > 0)
            baseTheta = (beatCount + beat) * beatSync * 2 * Math.PI;
        else
            baseTheta = t * speed * 2 * Math.PI;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            var fixturePhase = (n > 1)
                ? (i / (n - 1)) * phaseSpread * 2 * Math.PI
                : 0;
            var theta = baseTheta - fixturePhase;

            var pan  = (cx + panRadius  * Math.sin(pFreq * theta + delta)) * f.panRange;
            var tilt = (cy + tiltRadius * Math.sin(tFreq * theta))          * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                intent.dimmer = palettes.dimmer.dimmer;
            else if (f.hasDimmer)
                intent.dimmer = 1.0;

            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = palettes.color.r;
                intent.g = palettes.color.g;
                intent.b = palettes.color.b;
            }

            return intent;
        });
    };

    return effect;
})()
