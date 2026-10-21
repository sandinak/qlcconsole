/*
  QLC+ Effect Script: Matrix Fireworks
  Rockets climb from the bottom and burst into sparks that spread, fall and
  fade -- bursting on the beat, or going up at random.

  Pure & testable: each rocket worked out from the clock; where and when
  from a hash, not Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Fireworks";
    effect.description = "Rockets rise and burst into falling sparks";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Rockets climb from the bottom and burst into sparks that spread out, fall and fade, each burst in the next of the look's colours. Timing: bursts land on the beat (Division: how often; the rocket goes up just before), or go up at random at about Speed a second. Rise: how long a rocket takes to climb (seconds). Sparks: how many each burst throws. Size: how far the sparks fly (pixels). Fade: how long they last (seconds). On a wide canvas several go up at once, spread along it. The rocket climbs in its burst's colour, with a white spark at its head where there are white pixels. The accent is the flash of each burst and the rocket. Scope: across the whole canvas, or in each box; Each box, staggered: each box's fireworks a little behind the last (Stagger order; Stagger: how far, as a fraction of the time between bursts).";

    effect.parameters = [
        { name: "timing",   description: "Burst on the beat, or at random", defaultValue: 0, values: ["On the beat", "Random (speed)"] },
        { name: "division", description: "On the beat: how often one bursts", defaultValue: 1, values: MATRIX.DIVISIONS },
        { name: "speed",    description: "Random: rockets a second (per beat on a speed master)", min: 0.1, max: 4, defaultValue: 0.8 },
        { name: "rise",     description: "Seconds a rocket takes to climb", min: 0, max: 2, defaultValue: 0.35 },
        { name: "sparks",   description: "Sparks a burst throws", min: 3, max: 24, defaultValue: 12 },
        { name: "size",     description: "How far the sparks fly (pixels)", min: 1, max: 15, defaultValue: 5 },
        { name: "fade",     description: "Seconds the sparks last", min: 0.2, max: 3, defaultValue: 1.1 },
        { name: "scope",    description: "Across the whole canvas, or in each box", defaultValue: 0, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var onBeat0 = Math.round(MATRIX.num(params.timing, 0)) === 0;
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.8), MATRIX.perBeat(params.division)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var perBeat = MATRIX.perBeat(params.division);
            var speed = Math.max(0.01, num(params.speed, 0.8));
            var clk = MATRIX.clock(inputs, speed, onBeat0, perBeat);
            var pos = clk.pos;
            // Seconds in clock steps.
            var stepsPerSec = clk.beat ? perBeat * num(inputs._bpm, 120) / 60 : speed;
            var riseP = Math.max(0, num(params.rise, 0.35)) * stepsPerSec;
            var fadeP = Math.max(0.05, num(params.fade, 1.1)) * stepsPerSec;
            var sparks = Math.max(3, Math.round(num(params.sparks, 12)));
            var size = Math.max(0.5, num(params.size, 5));
            var perBox = MATRIX.perBox(params, 0);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 40, b: 40 }, { r: 60, g: 140, b: 255 },
                                              { r: 255, g: 200, b: 40 }, { r: 40, g: 255, b: 120 }]);
            var random = !clk.beat;
            var areas = {};

            // Every rocket and burst showing in a w x h area now.
            function showsOf(w, h) {
                var key = w + "x" + h;
                if (areas[key]) return areas[key];
                var list = [];
                var lanes = Math.max(1, Math.round(w / 40));
                var newest = Math.floor(pos + riseP + 1), oldest = Math.floor(pos - fadeP - 1);
                for (var k = newest; k >= oldest; k--) {
                    for (var b = 0; b < lanes; b++) {
                        var at = k + (random ? (MATRIX.hash(k, b, 1) - 0.5) * 0.8 : 0);
                        if (lanes > 1 && b > 0 && MATRIX.hash(k, b, 8) < 0.35) continue;   // not every lane every time
                        if (pos < at - riseP || pos > at + fadeP) continue;
                        var bx = ((b + 0.15 + 0.7 * MATRIX.hash(k, b, 2)) / lanes) * (w - 1);
                        var by = (0.12 + 0.33 * MATRIX.hash(k, b, 3)) * (h - 1);
                        var s = { k: k + b, bx: bx, by: by, c: MATRIX.pick(C, k * 3 + b) };
                        if (pos < at) {
                            var a = riseP > 0 ? 1 - (at - pos) / riseP : 1;
                            var ease = 1 - (1 - a) * (1 - a);
                            s.rocket = true;
                            s.ry = (h - 1) + (by - (h - 1)) * ease;
                            s.ty = Math.min(h - 1, s.ry + 1.4);
                        } else {
                            var age = (pos - at) / fadeP;
                            var R = size * (0.75 + 0.25 * MATRIX.hash(k, b, 4));
                            var travel = R * (1 - Math.pow(1 - Math.min(1, age * 1.4), 2));
                            var drop = 1.6 * age * age * Math.max(1, (h - 1) * 0.5);
                            s.age = age;
                            s.pts = [];
                            for (var j = 0; j < sparks; j++) {
                                var th = 2 * Math.PI * (j + 0.5 * MATRIX.hash(k, b, j + 10)) / sparks;
                                var sp = 0.7 + 0.3 * MATRIX.hash(k, j, b + 20);
                                s.pts.push({ x: bx + Math.cos(th) * travel * sp, y: by + Math.sin(th) * travel * sp + drop,
                                             tw: MATRIX.hash(k, j, Math.floor(pos * 9 / Math.max(0.01, stepsPerSec))) });
                            }
                            s.reach = R + 2;
                        }
                        list.push(s);
                    }
                }
                areas[key] = list;
                return list;
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var shows = showsOf(p.w, p.h);
                var best = 0, acc = 0, colour = C[0];
                for (var i = 0; i < shows.length; i++) {
                    var s = shows[i], lvl;
                    if (s.rocket) {
                        if (Math.abs(p.x - s.bx) > 1.5) continue;
                        var seg = MATRIX.segment(p.x, p.y, s.bx, s.ty, s.bx, s.ry);
                        lvl = MATRIX.cover(seg.d, 1) * (0.25 + 0.75 * seg.t) * 0.9;
                        if (lvl > best) { best = lvl; colour = s.c; }
                        acc = Math.max(acc, lvl * 0.6);
                        continue;
                    }
                    if (Math.abs(p.x - s.bx) > s.reach || p.y < s.by - s.reach) continue;
                    var life = Math.pow(1 - s.age, 1.5);
                    // The flash: a ball of light at the burst, gone in a moment.
                    if (s.age < 0.12) {
                        var dc = Math.sqrt((p.x - s.bx) * (p.x - s.bx) + (p.y - s.by) * (p.y - s.by));
                        var fl = MATRIX.cover(dc, 2.5 * (1 - s.age / 0.12) + 0.5);
                        acc = Math.max(acc, fl * (1 - s.age / 0.12));
                        if (fl > best) { best = fl; colour = s.c; }
                    }
                    for (var j = 0; j < s.pts.length; j++) {
                        var q = s.pts[j];
                        var dx = p.x - q.x, dy = p.y - q.y;
                        if (dx > 1.5 || dx < -1.5 || dy > 1.5 || dy < -1.5) continue;
                        lvl = MATRIX.cover(Math.sqrt(dx * dx + dy * dy), 1) * life;
                        if (s.age > 0.55) lvl *= q.tw > 0.45 ? 1 : 0.35;     // crackle as they die
                        if (lvl > best) { best = lvl; colour = s.c; }
                    }
                }
                return MATRIX.out(f, cv, colour, best, acc, params);
            });
        });
    };

    return effect;
})()
