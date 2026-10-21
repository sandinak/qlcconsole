/*
  QLC+ Effect Script: Matrix Bouncing Balls
  Balls of the look's colours bounce round each box (or the whole canvas)
  off its edges -- floating free, or dropping and bouncing on the floor --
  with a soft trail and a flash as they hit.

  Pure & testable: each ball's path worked out from inputs._time; the
  balls from a hash, not Math.random.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix Bouncing Balls";
    effect.description = "Balls bounce off the edges, with or without gravity";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.notes = "Balls bounce round inside the canvas, each in the next of the look's colours. Count: how many. Size: each ball's radius in pixels. Trail: how long a soft trail follows each. Gravity: off, they float and bounce off every edge; on, they drop and bounce on the floor as they travel across. Speed: how fast they go (crossings a second; per beat on a speed master). The accent is the flash as a ball hits. Scope: balls in each box, or across the whole canvas; Each box, staggered: each box's balls a little behind the last (Stagger order; Stagger: how far, as a fraction of a crossing).";

    effect.parameters = [
        { name: "speed",   description: "Crossings a second (per beat on a speed master)", min: 0.05, max: 3, defaultValue: 0.5 },
        { name: "count",   description: "How many balls", min: 1, max: 8, defaultValue: 3 },
        { name: "size",    description: "Each ball's radius (pixels)", min: 0.5, max: 4, defaultValue: 1 },
        { name: "trail",   description: "How long a trail follows each", min: 0, max: 1, defaultValue: 0.3 },
        { name: "gravity", description: "Float free, or drop and bounce on the floor", defaultValue: 1, values: ["Off", "On"] },
        { name: "scope",   description: "Balls in each box, or across the whole canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.staggerParams(), MATRIX.whiteParams(0));

    // 0..L..0..: a back-and-forth along a length L, at u.
    function tri(u, L) {
        if (L <= 0) return 0;
        var m = u - Math.floor(u / (2 * L)) * 2 * L;
        return L - Math.abs(m - L);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        return MATRIX.scoped(cv, fixtures, inputs, params, state,
                             MATRIX.cycle(1, MATRIX.num(params.speed, 0.5)),
                             function(fixtures, inputs) {
            var num = MATRIX.num;
            var t = num(inputs._time, 0);
            var speed = Math.max(0.001, num(params.speed, 0.5));
            var count = Math.max(1, Math.round(num(params.count, 3)));
            var size = Math.max(0.3, num(params.size, 1));
            var trail = MATRIX.clamp(num(params.trail, 0.3), 0, 1);
            var gravity = Math.round(num(params.gravity, 1)) === 1;
            var perBox = MATRIX.perBox(params, 1);
            var C = MATRIX.colours(palettes, [{ r: 255, g: 40, b: 0 }, { r: 0, g: 120, b: 255 }, { r: 0, g: 255, b: 80 }]);
            var TRAIL = 5;
            var areas = {};

            // Ball @p i's centre in a w x h area at time @p at, and how hard it is hitting.
            function ball(i, w, h, at) {
                var L = w - 1, H = h - 1;
                var vx = speed * Math.max(1, L) * (0.6 + 0.8 * MATRIX.hash(i, 1)) * (MATRIX.hash(i, 2) < 0.5 ? -1 : 1);
                var u = MATRIX.hash(i, 3) * 2 * Math.max(1, L) + vx * at;
                var x = tri(u, L), y, hit;
                var edgeX = L > 0 ? Math.min(x, L - x) : 99;
                if (gravity) {
                    var T = (0.9 + 0.6 * MATRIX.hash(i, 4)) / speed;      // seconds a bounce
                    var q = at / T + MATRIX.hash(i, 5);
                    q = q - Math.floor(q);
                    var top = H * (0.7 + 0.3 * MATRIX.hash(i, 6));
                    y = H - top * 4 * q * (1 - q);
                    hit = Math.max(0, 1 - Math.min(q, 1 - q) / 0.07);
                } else {
                    var vy = speed * Math.max(1, H) * (0.7 + 0.9 * MATRIX.hash(i, 7)) * (MATRIX.hash(i, 8) < 0.5 ? -1 : 1);
                    var v = MATRIX.hash(i, 9) * 2 * Math.max(1, H) + vy * at;
                    y = tri(v, H);
                    var edgeY = H > 0 ? Math.min(y, H - y) : 99;
                    hit = Math.max(0, 1 - Math.min(edgeX, edgeY) / 0.6);
                }
                if (gravity) hit = Math.max(hit, Math.max(0, 1 - edgeX / 0.6));
                return { x: x, y: y, hit: hit };
            }

            function ballsOf(w, h) {
                var key = w + "x" + h;
                if (areas[key]) return areas[key];
                var list = [];
                var step = trail * 0.12 / speed;
                for (var i = 0; i < count; i++) {
                    var pts = [];
                    for (var j = 0; j <= (trail > 0 ? TRAIL : 0); j++) pts.push(ball(i, w, h, t - j * step));
                    list.push(pts);
                }
                areas[key] = list;
                return list;
            }

            return fixtures.map(function(f) {
                var p = MATRIX.point(f, cv, perBox);
                var all = ballsOf(p.w, p.h);
                var best = 0, acc = 0, which = 0;
                for (var i = 0; i < all.length; i++) {
                    var pts = all[i];
                    for (var j = 0; j < pts.length; j++) {
                        var b = pts[j];
                        var dx = p.x - b.x, dy = p.y - b.y;
                        if (Math.abs(dx) > size + 1.5 || Math.abs(dy) > size + 1.5) continue;
                        var d = Math.sqrt(dx * dx + dy * dy);
                        var fade = j === 0 ? 1 : 0.55 * Math.pow(1 - j / (TRAIL + 1), 1.5);
                        var lvl = MATRIX.cover(d, size * 2 * (j === 0 ? 1 : 0.8)) * fade;
                        if (lvl > best) { best = lvl; which = i; }
                        if (j === 0) acc = Math.max(acc, MATRIX.cover(d, size * 2) * b.hit);
                    }
                }
                return MATRIX.out(f, cv, MATRIX.pick(C, which), best, acc, params);
            });
        });
    };

    return effect;
})()
