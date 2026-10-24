/*
  QLC+ Effect Script: Matrix Ripple
  Rings spread out from a point -- the middle of each box, of the whole rig,
  an edge, or somewhere new each time -- like drops on water. Free running,
  or one ring on every beat.

  Pure & testable: driven by inputs._time / the beat clock; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Ripple";
    effect.description = "Rings ripple out from the middle (of each box or the whole rig)";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Rings spread out from a point, each in the next of the look's colours, fading as they go. Origin: the middle, an edge, or a new place for every ring. Timing: rings on the beat (Division sets how often) or free at Speed rings a second. Spacing: how far a ring travels before the next starts; Width: a ring's thickness; Fade: how much it fades by the edge. Scope: from the middle of each box, or of the whole canvas. The accent is the newest ring. Each box, staggered: the same in every box, each a little behind the last, so it rolls across the rig -- Stagger order: left to right, right to left, out from the middle or random; Stagger: how far behind, as a fraction of a ring.";

    effect.parameters = [
        { name: "timing",   description: "A ring on the beat, or free at Speed", defaultValue: 1, values: MATRIX.TIMINGS },
        { name: "division", description: "On the beat: how often a ring starts", defaultValue: 2, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Free: rings a second (per beat on a speed master)", min: 0.1, max: 6, defaultValue: 1 },
        { name: "spacing",  description: "Pixels a ring travels before the next starts", min: 1, max: 30, defaultValue: 4 },
        { name: "width",    description: "Ring thickness (pixels)", min: 0.5, max: 6, defaultValue: 1.2 },
        { name: "fade",     description: "How much a ring fades by the edge", min: 0, max: 1, defaultValue: 0.6 },
        { name: "origin",   description: "Where the rings start", defaultValue: 0,
          values: ["Middle", "Left", "Right", "Top", "Bottom", "Anywhere"] },
        { name: "scope",    description: "From the middle of each box, or of the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 1), MATRIX.perBeat(params.division)),
                             function(fixtures, inputs, state, root) {
            var num = MATRIX.num;
            var onBeat = Math.round(num(params.timing, 1)) === 0;
            var clk = MATRIX.clock(inputs, num(params.speed, 1), onBeat, MATRIX.perBeat(params.division));
            var pos = clk.pos;
            var spacing = Math.max(0.5, num(params.spacing, 4));
            var width = Math.max(0.3, num(params.width, 1.2));
            var fade = MATRIX.clamp(num(params.fade, 0.6), 0, 1);
            var origin = Math.round(num(params.origin, 0));
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 0, g: 120, b: 255 }, { r: 0, g: 255, b: 200 }]);
            var newest = Math.floor(pos);

            function originOf(k, p, boxIndex) {
                var cx = (p.w - 1) / 2, cy = (p.h - 1) / 2;
                switch (origin) {
                    case 1: return { x: 0, y: cy };
                    case 2: return { x: p.w - 1, y: cy };
                    case 3: return { x: cx, y: 0 };
                    case 4: return { x: cx, y: p.h - 1 };
                    case 5: return { x: MATRIX.hash(k, boxIndex, 1) * (p.w - 1), y: MATRIX.hash(k, boxIndex, 2) * (p.h - 1) };
                    default: return { x: cx, y: cy };
                }
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var reach = Math.sqrt(p.w * p.w + p.h * p.h);       // past every corner
                var rings = Math.ceil((reach + width) / spacing) + 1;
                var boxIndex = (perBox && p.box) ? p.box.index : 0;
                var best = 0, which = newest, acc = 0;
                var kHi = newest, kLo = newest - rings + 1;
                var fixed = origin !== 5, dist = 0;
                if (fixed) {
                    // One origin for every ring: only the rings whose radius
                    // is within a width of this pixel can light it (one more
                    // each side for rounding; the test below still decides).
                    var o0 = originOf(0, p, boxIndex);
                    var dx0 = p.x - o0.x, dy0 = p.y - o0.y;
                    dist = Math.sqrt(dx0 * dx0 + dy0 * dy0);
                    kHi = Math.min(kHi, Math.floor(pos - (dist - width) / spacing) + 1);
                    kLo = Math.max(kLo, Math.ceil(pos - (dist + width) / spacing) - 1);
                }
                for (var k = kHi; k >= kLo; k--) {
                    var r = (pos - k) * spacing;                    // this ring's radius
                    if (r < 0) continue;
                    var d;
                    if (fixed) {
                        d = Math.abs(dist - r);
                    } else {
                        var o = originOf(k, p, boxIndex);
                        var dx = p.x - o.x, dy = p.y - o.y;
                        d = Math.abs(Math.sqrt(dx * dx + dy * dy) - r);
                    }
                    if (d > width) continue;
                    var lvl = (1 - d / width) * (1 - fade * Math.min(1, r / reach));
                    if (lvl > best) { best = lvl; which = k; }
                    if (k === newest && pos - k < 0.5) acc = Math.max(acc, (1 - d / width) * (1 - (pos - k) * 2));
                }
                return MATRIX.out(f, cv, MATRIX.pick(C, which), best, acc, params);
            });
        });
    };

    return effect;
})()
