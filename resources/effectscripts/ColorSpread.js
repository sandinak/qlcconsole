(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Spread";
    effect.description = "Distribute two colors across the fixture group";
    effect.notes       = "Drag a Color palette onto each color slot. 'Alternate' alternates\n"
                       + "odd/even fixtures. 'Gradient' blends smoothly across the group.\n"
                       + "'Position' blends based on each fixture's stage X position.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    effect.palettes = [
        { name: "color1", type: "Color",  optional: false, description: "First color" },
        { name: "color2", type: "Color",  optional: true,  description: "Second color (defaults to black)" },
        { name: "dimmer", type: "Dimmer", optional: true,  description: "Overall brightness" }
    ];

    effect.parameters = [
        {
            name: "mode", description: "Distribution mode",
            min: 0, max: 2, defaultValue: 0,
            values: ["Alternate", "Gradient", "Position"]
        },
        {
            name: "offset", description: "Phase / position offset (0–1)",
            min: 0.0, max: 1.0, defaultValue: 0.0
        }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var c1 = palettes.color1 && palettes.color1.r !== undefined
                 ? palettes.color1 : { r: 255, g: 255, b: 255 };
        var c2 = palettes.color2 && palettes.color2.r !== undefined
                 ? palettes.color2 : { r: 0, g: 0, b: 0 };
        var dm = (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                 ? palettes.dimmer.dimmer : 1.0;

        var mode   = Math.round(params.mode);
        var offset = params.offset || 0.0;
        var n      = fixtures.length;

        // Gather stage X extents for Position mode
        var minX = Infinity, maxX = -Infinity;
        if (mode === 2) {
            for (var k = 0; k < n; k++) {
                var px = fixtures[k].pos ? fixtures[k].pos.x : 0;
                if (px < minX) minX = px;
                if (px > maxX) maxX = px;
            }
            if (maxX <= minX) maxX = minX + 1;
        }

        return fixtures.map(function(f, i) {
            var t;
            if (mode === 0) {
                // Alternate: even=c1, odd=c2
                t = (i % 2 === 0) ? 0.0 : 1.0;
            } else if (mode === 1) {
                // Gradient: blend across fixture index
                t = (n > 1) ? (i / (n - 1) + offset) % 1.0 : 0.0;
            } else {
                // Position: blend by stage X
                var x = f.pos ? f.pos.x : 0;
                t = ((x - minX) / (maxX - minX) + offset) % 1.0;
            }

            var r = Math.round((c1.r + (c2.r - c1.r) * t) * dm);
            var g = Math.round((c1.g + (c2.g - c1.g) * t) * dm);
            var b = Math.round((c1.b + (c2.b - c1.b) * t) * dm);
            return { r: r, g: g, b: b };
        });
    };

    return effect;
})()
