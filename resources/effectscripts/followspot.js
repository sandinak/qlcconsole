/*
  QLC+ Effect Script: Followspot
  Move a set of fixtures using two joystick axes (pan/tilt).
  Compatible with any gamepad or MIDI controller mapped to the x/y inputs.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Followspot";
    effect.description = "Pan/tilt fixtures with joystick X/Y axes";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Maps joystick X/Y axes to pan and tilt so you can fly moving heads live. Bind the x and y inputs to a gamepad or MIDI controller. Sensitivity scales the range; a deadzone prevents drift near centre. Color and dimmer palettes are applied as static looks alongside the movement.";

    effect.inputs = [
        { name: "x", description: "Pan axis (0=left, 0.5=center, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Tilt axis (0=up, 0.5=center, 1=down)",   defaultValue: 0.5 }
    ];

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true },
        { name: "dimmer", type: "Dimmer", optional: true }
    ];

    effect.parameters = [
        { name: "sensitivity", description: "Pan/tilt travel scale", min: 0.1, max: 2.0, defaultValue: 1.0 },
        { name: "deadzone",    description: "Deadzone around center (0–0.5)", min: 0.0, max: 0.5, defaultValue: 0.05 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var x   = inputs.x !== undefined ? inputs.x : 0.5;
        var y   = inputs.y !== undefined ? inputs.y : 0.5;
        var dz  = params.deadzone    !== undefined ? params.deadzone    : 0.05;
        var sen = params.sensitivity !== undefined ? params.sensitivity : 1.0;

        // Apply deadzone symmetrically around 0.5
        function applyDeadzone(v) {
            var delta = v - 0.5;
            if (Math.abs(delta) < dz) return 0.5;
            var sign  = delta > 0 ? 1 : -1;
            var scale = (Math.abs(delta) - dz) / (0.5 - dz);
            return 0.5 + sign * scale * 0.5;
        }

        var xN = applyDeadzone(x);
        var yN = applyDeadzone(y);

        return fixtures.map(function(f) {
            var intent = {};
            if (f.hasPanTilt) {
                intent.pan  = xN * f.panRange  * sen;
                intent.tilt = yN * f.tiltRange * sen;
            }
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
