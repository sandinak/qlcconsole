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
    effect.fixtureTypes = ["rgb", "colorwheel"];
    effect.notes        = "Interpolates between two colours across the rig. The colours come from the look's first two Colour palettes (Red then Blue gives red→blue). Set which way the gradient runs with the direction picker — drag the arrow or use the →↓←↑ buttons (left→right, top→bottom, …); the preview shows the actual colours and direction. Works on additive RGB fixtures and on colour-wheel movers (mapped to the nearest wheel slot). Needs 3D positions in the 2D Monitor; falls back to array index when unplaced. The shift input slides the gradient live.";

    effect.inputs = [
        { name: "shift", description: "Shift the gradient left/right (0-1)", defaultValue: 0.0 }
    ];

    // No palette slots and no dimmer: this is a pure colour effect. The two
    // gradient ends come straight from the LOOK's Colour palettes (in order),
    // and intensity is left to the scene's own Dimmer look.
    effect.parameters = [
        { name: "angle",  description: "Gradient direction", type: "direction", min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "gamma",  description: "Gradient curve (1=linear, 2=ease)", min: 0.5, max: 4.0, defaultValue: 1.0  }
    ];

    // Sample an evenly-spaced multi-stop gradient at t in [0,1].
    function sampleStops(stops, t) {
        var n = stops.length;
        if (n === 1) return stops[0];
        if (t <= 0) return stops[0];
        if (t >= 1) return stops[n - 1];
        var x = t * (n - 1);
        var i = Math.floor(x);
        var f = x - i;
        var a = stops[i], b = stops[i + 1];
        return { r: a.r + (b.r - a.r) * f,
                 g: a.g + (b.g - a.g) * f,
                 b: a.b + (b.b - a.b) * f };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var shift = inputs.shift !== undefined ? inputs.shift : 0.0;
        var angle = params.angle  !== undefined ? params.angle  : 0.0;
        var gamma = params.gamma  !== undefined ? params.gamma  : 1.0;

        // Gradient stops = ALL the look's Colour palettes (precedence order),
        // so 3+ colours give a Red→Green→Blue spread. Fall back to red→blue.
        var L = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var stops = [];
        for (var s = 0; s < L.length; s++)
            if (L[s] && L[s].r !== undefined) stops.push(L[s]);
        if (stops.length === 0)
            stops = [{ r: 255, g: 0, b: 0 }, { r: 0, g: 0, b: 255 }];

        // Project each fixture's floor position onto the gradient direction.
        // angle is a SCREEN bearing (0=left→right, 90=top→bottom, matching the
        // 2D Monitor and the direction-picker widget). Stage +y is upstage =
        // screen UP, so we flip y (px=x, py=-y) before projecting.
        var rad = angle * Math.PI / 180.0;
        var dx = Math.cos(rad), dy = Math.sin(rad);
        function axisVal(f) {
            var px = (f.pos ? f.pos.x : 0);
            var py = (f.pos ? -f.pos.y : 0);
            return px * dx + py * dy;
        }

        // Find range of the projected coordinate
        var mn = Infinity, mx = -Infinity;
        for (var i = 0; i < fixtures.length; i++) {
            var v = axisVal(fixtures[i]);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        // Spread by position only when the fixtures actually differ along the
        // direction; otherwise (unplaced, or all on one line perpendicular to
        // it) fall back to array index so the gradient still spans the group
        // instead of collapsing to a single colour.
        var spreadByPos = (mx > mn);
        var range = spreadByPos ? (mx - mn) : 1.0;

        return fixtures.map(function(f, i) {
            // Normalised 0..1 position along chosen axis
            var raw = spreadByPos ? ((axisVal(f) - mn) / range)
                                  : (i / Math.max(fixtures.length - 1, 1));
            // Slide the gradient with shift, clamped so the endpoints stay the
            // two colours (a clean linear A→B map across the rig). Use
            // color-gradient.js when you want a wrapping/scrolling gradient.
            var t = raw + shift;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            // Apply gamma
            t = Math.pow(t, gamma);

            var c = sampleStops(stops, t);
            // Colour only — leave dimmer/pan/tilt to the scene's own looks.
            return { r: Math.round(c.r), g: Math.round(c.g), b: Math.round(c.b) };
        });
    };

    return effect;
})()
