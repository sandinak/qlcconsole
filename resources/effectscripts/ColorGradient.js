(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Gradient";
    effect.description = "Smooth gradient between two colors, optionally animated";
    effect.notes       = "Drag two Color palettes onto color1 and color2. Speed > 0 animates\n"
                       + "the gradient so it scrolls across the group over time.\n"
                       + "Speed = 0 gives a static gradient based on fixture index.";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "rgbw"];

    effect.palettes = [
        { name: "color1", type: "Color",  optional: false, description: "Start color" },
        { name: "color2", type: "Color",  optional: false, description: "End color" },
        { name: "dimmer", type: "Dimmer", optional: true,  description: "Overall brightness" }
    ];

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

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var c1 = palettes.color1 && palettes.color1.r !== undefined
                 ? palettes.color1 : { r: 255, g: 0, b: 0 };
        var c2 = palettes.color2 && palettes.color2.r !== undefined
                 ? palettes.color2 : { r: 0, g: 0, b: 255 };
        var dm = (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                 ? palettes.dimmer.dimmer : 1.0;

        var speed = params.speed || 0;
        var width = params.width || 1.0;
        var t     = inputs._time || 0;
        var n     = fixtures.length;
        var scroll = (speed * t) % 1.0;

        return fixtures.map(function(f, i) {
            var pos = (n > 1) ? (i / (n - 1)) / width + scroll : 0;
            pos = pos - Math.floor(pos); // wrap 0..1
            var r = Math.round((c1.r + (c2.r - c1.r) * pos) * dm);
            var g = Math.round((c1.g + (c2.g - c1.g) * pos) * dm);
            var b = Math.round((c1.b + (c2.b - c1.b) * pos) * dm);
            return { r: r, g: g, b: b };
        });
    };

    return effect;
})()
