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
    effect.notes = "Maps joystick X/Y axes to pan and tilt so you can fly moving heads live. Bind the x and y inputs to a gamepad or MIDI controller. Sensitivity and deadzone are configured in the HID device profile (I/O Manager). Person Height biases the tilt centre so the beam lands at the subject's head. Color and dimmer palettes are applied as static looks alongside the movement.";

    // The host injects joystick data automatically when a HID profile is active.
    // Manual input bindings (x / y) are kept as a fallback for non-HID controllers.
    effect.dataChannels = ["joystick"];

    effect.inputs = [
        { name: "x", description: "Pan axis fallback (0=left, 0.5=center, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Tilt axis fallback (0=up, 0.5=center, 1=down)",   defaultValue: 0.5 }
    ];

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true },
        { name: "dimmer", type: "Dimmer", optional: true }
    ];

    effect.parameters = [
        { name: "personHeight", description: "Height of person being followed (cm) — adjusts tilt centre", min: 50, max: 250, defaultValue: 170 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        // Prefer automatic joystick data channel (HID profile active) over
        // manual input bindings — fall back to inputs.x/y for other controllers.
        var joystick = data && data.joystick;
        var x  = joystick ? joystick.pan  : (inputs.x !== undefined ? inputs.x  : 0.5);
        var y  = joystick ? joystick.tilt : (inputs.y !== undefined ? inputs.y  : 0.5);
        // sensitivity / deadzone are applied by the host via the HID profile;
        // use conservative defaults here as a fallback.
        var dz  = 0.05;
        var sen = 1.0;
        var pH = params.personHeight !== undefined ? params.personHeight : 170;

        // Small tilt offset so the beam lands at the person's head, not feet.
        // 170 cm = neutral; taller people → beam aims slightly higher (less tilt).
        var tiltOffset = (170 - pH) / 300 * 0.12;

        // Apply deadzone symmetrically around 0.5
        function applyDeadzone(v) {
            var delta = v - 0.5;
            if (Math.abs(delta) < dz) return 0.5;
            var sign  = delta > 0 ? 1 : -1;
            var scale = (Math.abs(delta) - dz) / (0.5 - dz);
            return 0.5 + sign * scale * 0.5;
        }

        var xN = applyDeadzone(x);
        var yN = Math.min(1.0, Math.max(0.0, applyDeadzone(y) + tiltOffset));

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
