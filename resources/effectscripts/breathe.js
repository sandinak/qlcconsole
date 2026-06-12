/*
  QLC+ Effect Script: Breathe
  Slowly pulses fixture intensity up and down using a sine wave.
  Great for ambient/atmospheric looks or standby states.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Breathe";
    effect.description = "Slow sine-wave dimmer pulse on selected fixtures";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "moving", "rgb"];
    effect.notes = "Pulses the dimmer up and down with a smooth sine wave. Useful for heartbeat/breathing atmospheres. Speed controls cycles per second; minDim and maxDim set the floor and ceiling. Phase offset staggers fixtures so they breathe in a wave.";

    effect.inputs = [];   // no live inputs; driven by time only

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true }
    ];

    effect.parameters = [
        { name: "speed",   description: "Cycles per second",          min: 0.05, max: 2.0,  defaultValue: 0.2  },
        { name: "minDim",  description: "Minimum dimmer level (0-1)", min: 0.0,  max: 1.0,  defaultValue: 0.05 },
        { name: "maxDim",  description: "Maximum dimmer level (0-1)", min: 0.0,  max: 1.0,  defaultValue: 1.0  },
        { name: "phaseOffset", description: "Phase spread between fixtures (degrees)", min: 0.0, max: 360.0, defaultValue: 0.0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t     = inputs._time !== undefined ? inputs._time : 0;
        var speed = params.speed       !== undefined ? params.speed       : 0.2;
        var mn    = params.minDim      !== undefined ? params.minDim      : 0.05;
        var mx    = params.maxDim      !== undefined ? params.maxDim      : 1.0;
        var phOff = params.phaseOffset !== undefined ? params.phaseOffset : 0.0;

        var phaseStep = phOff / 360.0 / Math.max(fixtures.length - 1, 1);

        return fixtures.map(function(f, i) {
            var phase  = t * speed * 2 * Math.PI - i * phaseStep * 2 * Math.PI;
            var wave   = (Math.sin(phase) + 1) / 2; // 0..1
            var dimmer = mn + wave * (mx - mn);

            var intent = { dimmer: dimmer };

            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = Math.round(palettes.color.r * dimmer / mx);
                intent.g = Math.round(palettes.color.g * dimmer / mx);
                intent.b = Math.round(palettes.color.b * dimmer / mx);
            }

            return intent;
        });
    };

    return effect;
})()
