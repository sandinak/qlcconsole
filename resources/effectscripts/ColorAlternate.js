(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Alternate";
    effect.description = "Cycle up to four colors across the fixture group";
    effect.notes       = "Drag Color palettes onto color1–color4 slots. Unbound slots are\n"
                       + "skipped, so two bound palettes give a simple A/B split.\n"
                       + "Offset shifts which fixture starts on which color.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    effect.palettes = [
        { name: "color1", type: "Color", optional: false, description: "Color A" },
        { name: "color2", type: "Color", optional: true,  description: "Color B" },
        { name: "color3", type: "Color", optional: true,  description: "Color C" },
        { name: "color4", type: "Color", optional: true,  description: "Color D" },
        { name: "dimmer", type: "Dimmer", optional: true, description: "Overall brightness" }
    ];

    effect.parameters = [
        {
            name: "offset", description: "Starting fixture offset (integer)",
            min: 0, max: 16, defaultValue: 0
        }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var dm = (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                 ? palettes.dimmer.dimmer : 1.0;
        var offset = Math.round(params.offset) || 0;

        // Build the active color list (skip unbound optional slots)
        var colors = [];
        var slots = ["color1", "color2", "color3", "color4"];
        for (var s = 0; s < slots.length; s++) {
            var pal = palettes[slots[s]];
            if (pal && pal.r !== undefined)
                colors.push(pal);
        }
        if (colors.length === 0)
            colors.push({ r: 255, g: 255, b: 255 });

        return fixtures.map(function(f, i) {
            var c = colors[(i + offset) % colors.length];
            return {
                r: Math.round(c.r * dm),
                g: Math.round(c.g * dm),
                b: Math.round(c.b * dm)
            };
        });
    };

    return effect;
})()
