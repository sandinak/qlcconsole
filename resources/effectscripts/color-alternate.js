/*
  QLC+ Effect Script: Color Alternate
  Cycle up to four Color palettes across the fixture group (A/B/C/D split).
  Colour-only look generator: it drives r/g/b (scaled by an optional Dimmer
  palette) and leaves pan/tilt to the scene.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Alternate";
    effect.description = "Cycle up to four colors across the fixture group";
    effect.notes       = "Colours come from the LOOK: stack Colour palettes (in order) and they\n"
                       + "become the cycle — two give an A/B split, three A/B/C, etc. Offset\n"
                       + "shifts which fixture starts on which colour. Intensity is left to the\n"
                       + "scene's Dimmer look.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    // No palette slots: the cycle is the look's Colour palettes, in order.
    effect.parameters = [
        {
            name: "offset", description: "Starting fixture offset (integer)",
            min: 0, max: 16, defaultValue: 0
        }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var offset = params.offset !== undefined ? Math.round(params.offset) : 0;

        // The cycle is the look's Colour palettes, in precedence order.
        var colors = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        if (colors.length === 0)
            colors = [{ r: 255, g: 255, b: 255 }];

        return fixtures.map(function(f, i) {
            var c = colors[(i + offset) % colors.length];
            return { r: Math.round(c.r), g: Math.round(c.g), b: Math.round(c.b) };
        });
    };

    return effect;
})()
