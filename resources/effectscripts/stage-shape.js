/*
  QLC+ Effect Script: Stage Shape

  Lays a geometric shape out on the stage, centred on the scene's Aim look, and
  aims each moving head at a point on the shape's OUTLINE.  With enough heads the
  beams trace the whole figure on the deck (or stand it up on a vertical plane);
  with fewer, they sample evenly around the perimeter.

  Position only — composes with the scene
  ───────────────────────────────────────
  This effect controls POSITION ONLY.  Colour and dimmer come from the scene's
  own Color/Dimmer looks (it never touches those channels), and the shape's
  CENTRE is the scene's own Target (Aim) look — there is nothing to bind here.
  Add an Aim look to the scene, drop Stage Shape on top, and the heads fan out
  around it; move the target and the whole shape follows.  No Aim look → the
  effect does nothing (there is no anchor to build on).

  How it aims
  ───────────
  The shape lives in WORLD space (metres).  Each fixture is given a point on the
  outline and returns a world-space aim intent {aimX, aimY, aimZ}; the host runs
  it through the shared AimSolver (the same geometry the Aim palette uses) so
  every head — wherever it hangs — converges on its own point.  Needs fixtures
  placed in the 2D Monitor so the host can solve each aim.

  Parameters
  ──────────
  shape     – Circle / Triangle / Square / Pentagon / Hexagon / Star / Line
  size      – shape radius in metres
  rotation  – static rotation of the shape (degrees)
  spin      – animate it: closed shapes chase beams around the outline, the Line
              spins like a propeller (revolutions/sec, +/- for direction)
  plane     – Floor (flat on the deck) or Wall (stood up, facing downstage)
  height    – lift the shape off the target along the plane (metres)
*/

(function () {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Stage Shape";
    effect.description  = "Aim moving heads around the outline of a shape on stage";
    effect.notes        = "Position-only: centres on the scene's Target (Aim) "
                        + "look and aims each head at its own point on the shape's "
                        + "outline. Colour and dimmer stay with the scene's looks. "
                        + "Pick the shape and size (metres); 'plane' lays it flat "
                        + "on the deck or stands it up facing the audience; 'spin' "
                        + "walks the beams around the figure. Add an Aim look to "
                        + "the scene (the shape centre) and place fixtures in the "
                        + "2D Monitor so the host can solve each aim.";
    effect.fixtureTypes = ["moving"];
    effect.author       = "QLC+ Programmer";

    // No palettes: colour/dimmer belong to the scene, and the centre is the
    // scene's own Aim look (injected as fixture.sceneTarget), like the followspot.
    effect.palettes = [];

    effect.parameters = [
        { name: "shape",    description: "Shape",
          defaultValue: 0,
          values: ["Circle", "Triangle", "Square", "Pentagon", "Hexagon", "Star", "Line"] },
        { name: "size",     description: "Shape radius (metres)",
          min: 0.2, max: 15.0, defaultValue: 2.0 },
        { name: "rotation", description: "Static rotation (degrees)",
          min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "spin",     description: "Spin (revolutions/sec, +/- for direction)",
          min: -2.0, max: 2.0, defaultValue: 0.0 },
        { name: "plane",    description: "Orientation",
          defaultValue: 0, values: ["Floor", "Wall"] },
        { name: "height",   description: "Lift off the target along the plane (metres)",
          min: -5.0, max: 10.0, defaultValue: 0.0 }
    ];

    // --- Outline geometry, in a unit shape (extent ~1), centred on origin ------

    // Vertices of a closed k-gon, first vertex at the top, going clockwise.
    function polygon(k) {
        var v = [];
        for (var j = 0; j < k; j++) {
            var a = Math.PI / 2 - 2 * Math.PI * j / k;
            v.push([Math.cos(a), Math.sin(a)]);
        }
        return v;
    }

    // 5-point star: alternate outer (1.0) and inner (0.382) radii, 10 vertices.
    function star() {
        var v = [], inner = 0.382;
        for (var j = 0; j < 10; j++) {
            var r = (j % 2 === 0) ? 1.0 : inner;
            var a = Math.PI / 2 - 2 * Math.PI * j / 10;
            v.push([r * Math.cos(a), r * Math.sin(a)]);
        }
        return v;
    }

    // Build the closed-polygon vertex list for a shape index, or null for the
    // analytic shapes (circle / line) handled directly in the tick.
    function vertsForShape(idx) {
        switch (idx) {
            case 1: return polygon(3);  // Triangle
            case 2: return polygon(4);  // Square
            case 3: return polygon(5);  // Pentagon
            case 4: return polygon(6);  // Hexagon
            case 5: return star();      // Star
            default: return null;       // 0 Circle, 6 Line
        }
    }

    // Cumulative edge lengths around a closed polygon (for arc-length sampling).
    function cumulative(verts) {
        var n = verts.length, seg = [], total = 0;
        for (var j = 0; j < n; j++) {
            var a = verts[j], b = verts[(j + 1) % n];
            var dx = b[0] - a[0], dy = b[1] - a[1];
            var d = Math.sqrt(dx * dx + dy * dy);
            seg.push(d); total += d;
        }
        return { seg: seg, total: total };
    }

    // Point at fraction s in [0,1) around the closed polygon, evenly by length.
    function samplePoly(verts, cum, s) {
        var dist = s * cum.total, acc = 0, n = verts.length;
        for (var j = 0; j < n; j++) {
            if (acc + cum.seg[j] >= dist || j === n - 1) {
                var f = cum.seg[j] > 0 ? (dist - acc) / cum.seg[j] : 0;
                var a = verts[j], b = verts[(j + 1) % n];
                return [a[0] + f * (b[0] - a[0]), a[1] + f * (b[1] - a[1])];
            }
            acc += cum.seg[j];
        }
        return verts[0];
    }

    function rotate(p, deg) {
        var r = deg * Math.PI / 180, c = Math.cos(r), s = Math.sin(r);
        return [p[0] * c - p[1] * s, p[0] * s + p[1] * c];
    }

    effect.tick = function (fixtures, inputs, palettes, params, state) {
        var t        = inputs._time || 0;
        var shapeIdx = params.shape    !== undefined ? Math.round(params.shape)    : 0;
        var size     = params.size     !== undefined ? params.size                 : 2.0;
        var rotation = params.rotation !== undefined ? params.rotation             : 0.0;
        var spin     = params.spin     !== undefined ? params.spin                 : 0.0;
        var wall     = (params.plane   !== undefined ? Math.round(params.plane) : 0) === 1;
        var height   = params.height   !== undefined ? params.height               : 0.0;

        var verts  = vertsForShape(shapeIdx);
        var cum    = verts ? cumulative(verts) : null;
        var isLine = (shapeIdx === 6);

        var N = Math.max(fixtures.length, 1);

        return fixtures.map(function (f, i) {
            // Centre = the scene's own Aim look. No Aim look → nothing to build
            // on, so leave this head to the scene.
            var c = f.sceneTarget;
            if (!f.hasPanTilt || !c || c.x === undefined || c.y === undefined)
                return {};
            var cx = c.x, cy = c.y, cz = (c.z !== undefined ? c.z : 0.0);

            // Local outline point (unit shape) for this fixture.
            var local, spinDeg = 0.0;
            if (isLine) {
                // Open shape: spread heads end-to-end, spin the whole line.
                var s = (N > 1) ? i / (N - 1) : 0.5;
                local = [-1 + 2 * s, 0];
                spinDeg = spin * t * 360.0;
            } else {
                // Closed shape: spin walks the heads around the perimeter.
                var phase = ((i / N) + spin * t) % 1.0;
                if (phase < 0) phase += 1.0;
                local = verts ? samplePoly(verts, cum, phase)
                              : [Math.cos(2 * Math.PI * phase), Math.sin(2 * Math.PI * phase)];
            }

            // Orient, then scale to metres.
            local = rotate(local, rotation + spinDeg);
            var X = local[0] * size, Y = local[1] * size;

            // Place onto the chosen world plane around the target.
            var wx, wy, wz;
            if (wall) {
                // Vertical plane facing downstage: X across, Y up.
                wx = cx + X;  wy = cy;          wz = cz + height + Y;
            } else {
                // Floor plane: X stage-right, Y upstage, flat on the deck.
                wx = cx + X;  wy = cy + Y;      wz = cz + height;
            }

            // Position only — colour/dimmer come from the scene's looks.
            return { aimX: wx, aimY: wy, aimZ: wz };
        });
    };

    return effect;
})()
