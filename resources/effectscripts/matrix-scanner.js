/*
  QLC+ Effect Script: Matrix Scanner
  The KITT scanner: a bar sweeps back and forth, easing at each end, a
  fading tail behind it -- across or up and down, one or several.

  Pure & testable: driven by inputs._time; no Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Scanner";
    effect.description = "A bar sweeps back and forth with a fading tail (KITT)";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "A bar sweeps back and forth with a tail fading behind it, like KITT's scanner -- red, or the look's colours (one a bar). Direction: across, or up and down. Count: how many bars, spread through the sweep. Width: the bar's width in pixels. Tail: how long the tail lasts, as a part of a sweep. Motion: smooth (easing at each end, the classic) or straight. Speed: sweeps (there and back) a second (per beat on a speed master). The accent is the bar's bright core. Scope: in each box, or one sweep across the whole canvas; Each box, staggered: each box's scanner a little behind the last (Stagger order; Stagger: how far, as a fraction of a sweep).";

    effect.parameters = [
        { name: "speed",     description: "Sweeps a second, there and back (per beat on a speed master)", min: 0.05, max: 4, defaultValue: 0.6 },
        { name: "direction", description: "Which way it sweeps", defaultValue: 0, values: ["Across", "Up and down"] },
        { name: "count",     description: "How many bars", min: 1, max: 4, defaultValue: 1 },
        { name: "width",     description: "The bar's width (pixels)", min: 0.5, max: 4, defaultValue: 1.5 },
        { name: "tail",      description: "How long the tail lasts (part of a sweep)", min: 0, max: 1, defaultValue: 0.35 },
        { name: "motion",    description: "How it moves", defaultValue: 0, values: ["Smooth (eases at the ends)", "Straight"] },
        { name: "scope",     description: "In each box, or one sweep across the canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.6)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var across = Math.round(num(params.direction, 0)) === 0;
            var count = Math.max(1, Math.round(num(params.count, 1)));
            var width = Math.max(0.3, num(params.width, 1.5));
            var tail = MATRIX.clamp(num(params.tail, 0.35), 0, 1);
            var smooth = Math.round(num(params.motion, 0)) === 0;
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 0, b: 0 }]);
            var phase = num(inputs._time, 0) * num(params.speed, 0.6);
            var SAMPLES = 14;

            // Where bar k is along a run of L pixels at phase @p ph.
            function at(ph, L) {
                var q = ph - Math.floor(ph);
                var tr = q < 0.5 ? q * 2 : 2 - q * 2;                // 0..1..0
                if (smooth) tr = 0.5 - 0.5 * Math.cos(Math.PI * tr);
                return tr * Math.max(0, L - 1);
            }

            // Each bar's place now and where it was, along a run of L pixels.
            var runs = {};
            function trailOf(L) {
                if (runs[L]) return runs[L];
                var bars = [];
                for (var k = 0; k < count; k++) {
                    var ph = phase + k / count;
                    var pts = [{ c: at(ph, L), w: 1 }];
                    if (tail > 0)
                        for (var j = 1; j <= SAMPLES; j++) {
                            var back = j / SAMPLES;
                            pts.push({ c: at(ph - back * tail / 2, L), w: Math.pow(1 - back, 2) * 0.85 });
                        }
                    bars.push(pts);
                }
                runs[L] = bars;
                return bars;
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var L = across ? p.w : p.h;
                var s = across ? p.x : p.y;
                var bars = trailOf(L);
                var best = 0, acc = 0, which = 0;
                for (var k = 0; k < bars.length; k++) {
                    var pts = bars[k];
                    var lvl = 0;
                    for (var j = 0; j < pts.length; j++) {
                        var tl = MATRIX.cover(Math.abs(s - pts[j].c), width) * pts[j].w;
                        if (tl > lvl) lvl = tl;
                    }
                    acc = Math.max(acc, MATRIX.cover(Math.abs(s - pts[0].c), width * 0.5) * 0.8);
                    if (lvl > best) { best = lvl; which = k; }
                }
                return MATRIX.out(f, cv, MATRIX.pick(C, which), best, acc, params);
            });
        });
    };

    return effect;
})()
