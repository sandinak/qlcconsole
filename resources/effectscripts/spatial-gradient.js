/*
  QLC+ Effect Script: Spatial Gradient
  Interpolates between two color palettes based on each fixture's
  physical X position (stage left→right by default).
  Requires MonitorProperties position data for fixtures.
  Falls back to array-index order if no position data is set.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Spatial Gradient";
    effect.description = "Color gradient across the rig based on fixture 3D position";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb"];
    effect.notes        = "Interpolates between two colour palettes based on each fixture's physical stage position. Requires 3D positions set in the 2D Monitor (Edit > Monitor Properties). Falls back to array index when no position data is available. The shift input slides the gradient across the rig live.";

    effect.inputs = [
        { name: "shift", description: "Shift the gradient left/right (0-1)", defaultValue: 0.5 }
    ];

    effect.palettes = [
        { name: "colorA", type: "Color", optional: false },
        { name: "colorB", type: "Color", optional: false }
    ];

    effect.parameters = [
        { name: "axis",   description: "Sort axis: 0=X, 0.5=Y, 1=Z",       min: 0.0, max: 1.0, defaultValue: 0.0  },
        { name: "dimmer", description: "Dimmer level",                       min: 0.0, max: 1.0, defaultValue: 1.0  },
        { name: "gamma",  description: "Gradient curve (1=linear, 2=ease)", min: 0.5, max: 4.0, defaultValue: 1.0  }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var shift = inputs.shift !== undefined ? inputs.shift : 0.5;
        var axis  = params.axis   !== undefined ? params.axis   : 0.0;
        var dim   = params.dimmer !== undefined ? params.dimmer : 1.0;
        var gamma = params.gamma  !== undefined ? params.gamma  : 1.0;

        var ca = (palettes.colorA && palettes.colorA.r !== undefined)
                  ? palettes.colorA : { r: 255, g: 0, b: 0 };
        var cb = (palettes.colorB && palettes.colorB.r !== undefined)
                  ? palettes.colorB : { r: 0, g: 0, b: 255 };

        // Pick the coordinate to use for sorting
        function axisVal(f) {
            if (axis < 0.25) return f.pos.x;
            if (axis < 0.75) return f.pos.y;
            return f.pos.z;
        }

        // Find range of the chosen axis
        var mn = Infinity, mx = -Infinity;
        for (var i = 0; i < fixtures.length; i++) {
            var v = axisVal(fixtures[i]);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        var range = (mx > mn) ? (mx - mn) : 1.0;

        return fixtures.map(function(f, i) {
            // Normalised 0..1 position along chosen axis
            var raw = (range > 0) ? ((axisVal(f) - mn) / range) : (i / Math.max(fixtures.length - 1, 1));
            // Apply shift (wrapping)
            var t = (raw + shift) % 1.0;
            // Apply gamma
            t = Math.pow(t, gamma);

            var r = Math.round(ca.r + t * (cb.r - ca.r));
            var g = Math.round(ca.g + t * (cb.g - ca.g));
            var b = Math.round(ca.b + t * (cb.b - ca.b));

            var intent = { r: r, g: g, b: b };
            if (f.hasDimmer) intent.dimmer = dim;
            return intent;
        });
    };

    return effect;
})()
