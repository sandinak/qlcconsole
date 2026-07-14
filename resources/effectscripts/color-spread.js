/*
  QLC+ Effect Script: Color Spread
  Distribute two Color palettes across the group — alternate odd/even, blend
  by fixture index, or blend by stage X position. Colour-only look generator:
  drives r/g/b (scaled by an optional Dimmer palette), leaves pan/tilt alone.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Spread";
    effect.description = "Distribute two colors across the fixture group";
    effect.notes       = "Colours come from the LOOK: stack two Colour palettes. 'Alternate'\n"
                       + "alternates odd/even fixtures. 'Gradient' blends smoothly across the\n"
                       + "group. 'Position' blends based on each fixture's stage X position.\n"
                       + "Intensity is left to the scene's Dimmer look.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    // No palette slots: the two colours come from the look's Colour palettes.
    effect.parameters = [
        {
            name: "mode", description: "Distribution mode",
            defaultValue: 0,
            values: ["Alternate", "Gradient", "Position"]
        },
        {
            name: "offset", description: "Phase / position offset (0–1)",
            min: 0.0, max: 1.0, defaultValue: 0.0
        }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var c1 = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 };
        var c2 = (L[1] && L[1].r !== undefined) ? L[1] : { r: 0, g: 0, b: 0 };

        var mode   = params.mode   !== undefined ? Math.round(params.mode) : 0;
        var offset = params.offset !== undefined ? params.offset : 0.0;
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

            var r = Math.round(c1.r + (c2.r - c1.r) * t);
            var g = Math.round(c1.g + (c2.g - c1.g) * t);
            var b = Math.round(c1.b + (c2.b - c1.b) * t);
            return { r: r, g: g, b: b };
        });
    };

    return effect;
})()
