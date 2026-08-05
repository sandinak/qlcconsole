/*
  QLC+ Effect Script: Meteor
  A bright head sweeps across the pixels leaving a fading trail — the classic
  FastLED / WLED "Meteor". Travels along the fixture group's grid layout (or a
  1-D strip if the group has no layout), so it works on an LED bar, a pixel
  matrix (row-major), or any row of fixtures.

  Pure & testable: motion is driven by inputs._time; no Math.random / wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Meteor";
    effect.description = "A meteor with a fading trail sweeps across the pixels";
    effect.author      = "WLED/FastLED idea, port for QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.notes = "A bright head travels across the fixtures, using the group's grid layout (or a 1-D strip), leaving a fading tail. Colour comes from the look's first Colour palette. Speed sets travel rate; Trail sets tail length; Ends chooses Wrap (loops) or Bounce (reverses at the ends).";

    effect.parameters = [
        { name: "speed", description: "Cells per second",     min: 0.5, max: 60, defaultValue: 12 },
        { name: "trail", description: "Trail length (cells)", min: 1,   max: 40, defaultValue: 8  },
        { name: "ends",  description: "At the ends",          defaultValue: 0, values: ["Wrap", "Bounce"] }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n = fixtures.length;
        if (n === 0) return [];

        var g0    = fixtures[0].grid || { cols: n, rows: 1 };
        var cols  = g0.cols || n;
        var rows  = g0.rows || 1;
        var total = Math.max(1, cols * rows);

        var t      = inputs._time !== undefined ? inputs._time : 0;
        var speed  = params.speed !== undefined ? params.speed : 12;
        var trail  = Math.max(1, params.trail !== undefined ? params.trail : 8);
        var bounce = ((params.ends | 0) === 1);

        // Head position along the flattened (row-major) cell index.
        var head;
        if (bounce) {
            var span = (total - 1) || 1;
            var ph   = (t * speed) % (2 * span);
            head = ph <= span ? ph : (2 * span - ph);
        } else {
            head = (t * speed) % total;
        }

        var L    = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var col0 = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 };

        return fixtures.map(function(f) {
            var g   = f.grid || { col: 0, row: 0, cols: cols, rows: rows };
            var idx = g.col + g.row * cols;
            var d   = Math.abs(idx - head);
            if (!bounce) {                      // wrap-around distance
                var w = total - d;
                if (w < d) d = w;
            }
            var b = d < trail ? (1 - d / trail) : 0;
            var intent = {
                r: Math.round(col0.r * b),
                g: Math.round(col0.g * b),
                b: Math.round(col0.b * b)
            };
            if (f.hasDimmer) intent.dimmer = b;
            return intent;
        });
    };

    return effect;
})()
