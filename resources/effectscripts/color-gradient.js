/*
  QLC+ Effect Script: Color Gradient
  Smooth gradient between two Color palettes across the group, optionally
  scrolling over time. Colour-only look generator: it drives r/g/b (scaled by
  an optional Dimmer palette) and leaves pan/tilt to the scene.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Gradient";
    effect.description = "Smooth gradient between two colors, optionally animated";
    effect.notes       = "Colours come from the LOOK: stack two Colour palettes (e.g. Red then\n"
                       + "Blue) and the gradient runs between them. Speed > 0 scrolls the gradient\n"
                       + "across the group over time; Speed = 0 is static by fixture index.\n"
                       + "Intensity is left to the scene's Dimmer look.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    // No palette slots: the two ends come from the look's Colour palettes.
    effect.parameters = [
        {
            name: "speed", description: "Scroll speed (0 = static)",
            min: -5.0, max: 5.0, defaultValue: 0.0
        },
        {
            name: "width", description: "Gradient width relative to group (0.1–2.0)",
            min: 0.1, max: 2.0, defaultValue: 1.0
        }
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
        // Gradient stops = ALL the look's Colour palettes (precedence order).
        var L = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var stops = [];
        for (var s = 0; s < L.length; s++)
            if (L[s] && L[s].r !== undefined) stops.push(L[s]);
        if (stops.length === 0)
            stops = [{ r: 255, g: 0, b: 0 }, { r: 0, g: 0, b: 255 }];

        var speed = params.speed !== undefined ? params.speed : 0.0;
        var width = params.width !== undefined ? params.width : 1.0;
        var t     = inputs._time !== undefined ? inputs._time : 0;
        var n     = fixtures.length;
        var scroll = (speed * t) % 1.0;

        return fixtures.map(function(f, i) {
            var pos = (n > 1) ? (i / (n - 1)) / width + scroll : 0;
            pos = pos - Math.floor(pos); // wrap 0..1
            var c = sampleStops(stops, pos);
            return { r: Math.round(c.r), g: Math.round(c.g), b: Math.round(c.b) };
        });
    };

    return effect;
})()
