/*
  Drawn Path — effect script for QLC+ (programmer-mode fork)

  Fixtures follow a closed movement path drawn interactively in the Look Editor.
  The path is a Catmull-Rom spline through the control points; each fixture is
  offset along the path by its index so the full group fans out into a ribbon.

  Parameters
  ──────────
  path    – XY control-point array (drawn in the editor; default is a circle)
  speed   – cycles per second  [0.01 – 2.0,  default 0.25]
  spread  – fraction of the path between adjacent fixtures  [0.0 – 1.0,  default 0.5]
  dimmer  – overall intensity when no dimmer palette is bound  [0.0 – 1.0, default 1.0]

  Inputs / palettes
  ─────────────────
  color   – optional Color palette (all fixtures share it)
  dimmer  – optional Dimmer palette (overrides the 'dimmer' parameter)
*/

(function () {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Drawn Path";
    effect.description  = "Fixtures follow a path drawn in the look editor";
    effect.notes        = "Click on the path canvas below to add control points, "
                        + "drag to reposition them, right-click to remove them. "
                        + "The closed Catmull-Rom spline through the points is the "
                        + "actual movement path. Use 'Circle' or 'Fig-8' presets to "
                        + "start from a common shape.";
    effect.fixtureTypes = ["moving"];
    effect.author       = "QLC+ Programmer";

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true },
        { name: "dimmer", type: "Dimmer", optional: true }
    ];

    effect.parameters = [
        { name: "path",
          description: "Movement path — draw in the canvas below",
          type: "path" },
        { name: "speed",
          description: "Cycle speed (cycles per second)",
          min: 0.01, max: 2.0, defaultValue: 0.25 },
        { name: "spread",
          description: "Phase spread between adjacent fixtures (0 = all together, 1 = spread across full path)",
          min: 0.0, max: 1.0, defaultValue: 0.5 },
        { name: "dimmer",
          description: "Intensity when no dimmer palette is bound",
          min: 0.0, max: 1.0, defaultValue: 1.0 }
    ];

    // Catmull-Rom interpolation between p1 and p2 using p0 and p3 as tangent helpers.
    // p0-p3 are 2-element arrays [x, y] in normalised [0, 1] space.
    function catmullRom(p0, p1, p2, p3, t) {
        var t2 = t * t, t3 = t2 * t;
        function cr(a, b, c, d) {
            return 0.5 * ((-a + 3*b - 3*c + d)*t3
                        + ( 2*a - 5*b + 4*c - d)*t2
                        + (-a + c)*t + 2*b);
        }
        return [cr(p0[0], p1[0], p2[0], p3[0]),
                cr(p0[1], p1[1], p2[1], p3[1])];
    }

    // Default path: circle with 8 points (used when no path has been drawn yet)
    function defaultCircle() {
        var pts = [];
        for (var i = 0; i < 8; i++) {
            var a = i * 2 * Math.PI / 8;
            pts.push([0.5 + 0.35 * Math.cos(a),
                      0.5 + 0.35 * Math.sin(a)]);
        }
        return pts;
    }

    effect.tick = function (fixtures, inputs, palettes, params, state) {
        var path   = (params.path && params.path.length >= 2) ? params.path : defaultCircle();
        var t      = inputs._time  || 0;
        var speed  = (params.speed  !== undefined) ? params.speed  : 0.25;
        var spread = (params.spread !== undefined) ? params.spread : 0.5;
        var dim    = (params.dimmer !== undefined) ? params.dimmer : 1.0;
        var N      = Math.max(fixtures.length, 1);
        var nSeg   = path.length;  // closed: last segment wraps to index 0

        // resolve dimmer from bound palette if available
        var dimFromPal = null;
        if (palettes.dimmer !== null && palettes.dimmer !== undefined
                && palettes.dimmer.dimmer !== undefined)
            dimFromPal = palettes.dimmer.dimmer;

        return fixtures.map(function (f, i) {
            if (!f.hasPanTilt) return {};

            // Fractional position on the path for this fixture
            var phase = (t * speed + (i * spread) / N) % 1.0;
            if (phase < 0) phase += 1.0;

            var rawIdx = phase * nSeg;
            var seg    = Math.floor(rawIdx) % nSeg;
            var frac   = rawIdx - Math.floor(rawIdx);

            var p0 = path[(seg - 1 + nSeg) % nSeg];
            var p1 = path[seg];
            var p2 = path[(seg + 1) % nSeg];
            var p3 = path[(seg + 2) % nSeg];

            var pos = catmullRom(p0, p1, p2, p3, frac);
            var x = Math.max(0, Math.min(1, pos[0]));
            var y = Math.max(0, Math.min(1, pos[1]));

            var intent = {
                pan:    x * f.panRange,
                tilt:   y * f.tiltRange,
                dimmer: dimFromPal !== null ? dimFromPal : dim
            };

            if (palettes.color !== null && palettes.color !== undefined) {
                if (palettes.color.r !== undefined) intent.r = palettes.color.r;
                if (palettes.color.g !== undefined) intent.g = palettes.color.g;
                if (palettes.color.b !== undefined) intent.b = palettes.color.b;
            }

            return intent;
        });
    };

    return effect;
})()
