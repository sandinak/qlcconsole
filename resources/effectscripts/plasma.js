/*
  QLC+ Effect Script: Plasma
  Classic 2-D plasma — smooth, shifting colour fields computed from the pixel's
  (col,row) grid position and time. Showcases the grid context: on a pixel
  matrix it's a true 2-D plasma; on a 1-D strip it degrades to a flowing hue
  wave. Colours span the hue wheel (Rainbow), or map onto the look's palette.

  Pure & testable: driven by inputs._time; no Math.random / wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Plasma";
    effect.description = "Smooth shifting 2-D colour fields across the pixel grid";
    effect.author      = "Demoscene idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.notes = "A flowing plasma computed from each fixture's grid cell. On a matrix it's a 2-D plasma; on a strip it's a hue wave. Scale sets the pattern size, Speed the flow rate. Colours: Rainbow spans the hue wheel; Look palette maps the plasma onto the look's stacked colours.";

    effect.parameters = [
        { name: "scale",  description: "Pattern size (smaller = larger blobs)", min: 0.05, max: 1.0, defaultValue: 0.35 },
        { name: "speed",  description: "Flow rate",                             min: 0.1,  max: 5.0, defaultValue: 1.0  },
        { name: "colors", description: "Colour source",                         defaultValue: 0, values: ["Rainbow", "Look palette"] }
    ];

    function hsv(h) {
        h = h - Math.floor(h);                 // wrap to 0..1
        var i = Math.floor(h * 6), f = h * 6 - i, q = 1 - f, t = f, r, g, b;
        switch (i % 6) {
            case 0:  r = 1; g = t; b = 0; break;
            case 1:  r = q; g = 1; b = 0; break;
            case 2:  r = 0; g = 1; b = t; break;
            case 3:  r = 0; g = q; b = 1; break;
            case 4:  r = t; g = 0; b = 1; break;
            default: r = 1; g = 0; b = q; break;
        }
        return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
    }

    // Blend across a list of look colours by a 0..1 position.
    function lookColor(L, u) {
        if (L.length === 1) return L[0];
        var x  = (u - Math.floor(u)) * (L.length - 1);
        var i  = Math.floor(x), f = x - i;
        var a  = L[i], b = L[Math.min(i + 1, L.length - 1)];
        return { r: Math.round(a.r + (b.r - a.r) * f),
                 g: Math.round(a.g + (b.g - a.g) * f),
                 b: Math.round(a.b + (b.b - a.b) * f) };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        if (fixtures.length === 0) return [];

        var t      = inputs._time !== undefined ? inputs._time : 0;
        var scale  = params.scale !== undefined ? params.scale : 0.35;
        var speed  = params.speed !== undefined ? params.speed : 1.0;
        var useLook = ((params.colors | 0) === 1);
        var L = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var haveLook = useLook && L.length > 0;

        var ts = t * speed;

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var x = g.col, y = g.row;
            // Sum of a few sine fields → smooth plasma value, normalised 0..1.
            var v = Math.sin(x * scale + ts)
                  + Math.sin(y * scale + ts * 1.3)
                  + Math.sin((x + y) * scale * 0.7 + ts * 0.7)
                  + Math.sin(Math.sqrt(x * x + y * y) * scale + ts * 1.1);
            var u = (v / 4 + 1) / 2;            // 0..1
            var col = haveLook ? lookColor(L, u) : hsv(u);
            var intent = { r: col.r, g: col.g, b: col.b };
            if (f.hasDimmer) intent.dimmer = 1.0;   // plasma is full-field; colour carries it
            return intent;
        });
    };

    return effect;
})()
