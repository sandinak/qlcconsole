/*
  QLC+ Effect Script: Fan
  Spreads fixtures in a fan from a joystick-controlled center position.
  The tilt is shared; pan spreads symmetrically around the center.
  Classic moving-head look for side-lighting, toplight arrays, etc.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Fan";
    effect.description = "Spread fixtures in a pan fan from joystick center";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Spreads fixtures in a static pan fan around a joystick-controlled centre. Tilt is shared across all fixtures. Best used with a symmetric linear array of moving heads at an even hang height. Spread controls total fan width in degrees; tiltBias nudges the shared tilt.";

    effect.inputs = [
        { name: "x", description: "Fan center pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Fan tilt        (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "spread",    description: "Total pan spread (degrees)",    min: 0.0,  max: 360.0, defaultValue: 90.0  },
        { name: "tiltBias",  description: "Extra tilt offset (degrees)",   min: -90.0, max: 90.0, defaultValue: 0.0   },
        { name: "invert",    description: "Invert fan order (0=no, 1=yes)", min: 0.0,  max: 1.0,  defaultValue: 0.0   }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var cx     = inputs.x !== undefined ? inputs.x : 0.5;
        var cy     = inputs.y !== undefined ? inputs.y : 0.5;

        var spread    = params.spread   !== undefined ? params.spread   : 90.0;
        var tiltBias  = params.tiltBias !== undefined ? params.tiltBias : 0.0;
        var inv       = params.invert   !== undefined ? params.invert   : 0.0;

        var n = Math.max(fixtures.length, 1);

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            // Normalised position −0.5..+0.5 symmetrically across the array
            var idx = (inv > 0.5) ? (n - 1 - i) : i;
            var pos = (n > 1) ? (idx / (n - 1) - 0.5) : 0;

            var panDeg  = cx * f.panRange  + pos * spread;
            var tiltDeg = cy * f.tiltRange + tiltBias;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  panDeg)),
                tilt: Math.max(0, Math.min(f.tiltRange, tiltDeg))
            };

            // Position effect: only touch dimmer/colour when a palette is
            // bound. Otherwise omit those keys so the scene's own Dimmer/Color
            // looks fall through (Override only wins channels we emit).
            if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                intent.dimmer = palettes.dimmer.dimmer;

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
