/*
  QLC+ Effect Script: Color Chase
  A lit band of color (from the color palette) chases through the fixture
  array.  Fixtures outside the band fade to the background color (or off).
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Color Chase";
    effect.description = "A color band chases through the fixture array";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "moving"];
    effect.notes = "A coloured band chases through the fixture array. The band width and speed are adjustable. Uses additive RGB on wash fixtures; maps to the nearest colour-wheel slot on moving heads. Add a background colour palette to keep out-of-band fixtures lit.";

    effect.inputs = [];

    // Colours come from the LOOK: the first Colour palette is the chase band,
    // the second (optional) is the background.

    effect.parameters = [
        { name: "speed",   description: "Chase speed (fixtures/sec)", min: 0.1, max: 20.0, defaultValue: 3.0  },
        { name: "width",   description: "Band width (0-1 of array)",  min: 0.0, max: 1.0,  defaultValue: 0.25 },
        { name: "dimmer",  description: "Peak dimmer",                min: 0.0, max: 1.0,  defaultValue: 1.0  },
        { name: "reverse", description: "Reverse direction (0=fwd, 1=rev)", min: 0.0, max: 1.0, defaultValue: 0.0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = inputs._time !== undefined ? inputs._time : 0;
        var speed  = params.speed   !== undefined ? params.speed   : 3.0;
        var width  = params.width   !== undefined ? params.width   : 0.25;
        var dimmer = params.dimmer  !== undefined ? params.dimmer  : 1.0;
        var rev    = params.reverse !== undefined ? params.reverse : 0.0;

        var n = fixtures.length;
        if (n === 0) return [];

        // Head position: 0..1, wraps
        var head = (t * speed / n) % 1.0;
        if (rev > 0.5) head = 1.0 - head;

        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var fc = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 };
        var bg = (L[1] && L[1].r !== undefined) ? L[1] : null;

        return fixtures.map(function(f, i) {
            var pos = i / n;  // fixture's normalised position

            // Toroidal distance to head
            var d = Math.abs(pos - head);
            if (d > 0.5) d = 1.0 - d;

            var halfW = width / 2;
            var bright = (d < halfW) ? (1.0 - d / halfW) : 0.0;
            bright = bright * bright; // square for sharper edges

            var intent = {};
            if (f.hasDimmer)
                intent.dimmer = (bg ? dimmer : bright * dimmer);

            if (bright > 0) {
                intent.r = Math.round(fc.r * bright);
                intent.g = Math.round(fc.g * bright);
                intent.b = Math.round(fc.b * bright);
            } else if (bg) {
                intent.r = bg.r; intent.g = bg.g; intent.b = bg.b;
            } else {
                intent.r = 0; intent.g = 0; intent.b = 0;
            }

            return intent;
        });
    };

    return effect;
})()
