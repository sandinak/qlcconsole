/*
  QLC+ Effect Script: Fire Spread
  The stage catches fire from one point. The head nearest the look's Aim
  target (centre stage when the look has none) catches first, and the fire
  runs outward at an even speed across the floor until every head burns;
  then it keeps burning, with Firelight's flicker, for as long as the look
  plays.

  The front: a head's distance from the point over the farthest head's
  (fixture.fromTarget / inputs._fromTargetFar, worked out by the host from
  where the heads stand -- each pixel of a pixel bar by its own place) is
  how far through the Spread time it catches, so a head twice as far
  catches twice as late. Edge makes the front ragged: each head catches a
  little early or late (a seeded hash, so it is repeatable) and kindles --
  a faint ember flicker -- just before. Catching is a flare, brighter and
  whiter, that settles into the steady fire within a second.

  The spread is the opening, timed from the effect's start (inputs._time),
  not a cycle: run on Loop it keeps burning and never starts again from the
  point, and a next cue that tracks it keeps it burning. Started again
  after it has stopped, it lights again from the point.

  Before the front a head is dark: dimmer 0, which the host takes as "none
  of the look's level". Moving heads get colour and dimmer only.

  Pure & testable: no Math.random and no state; the flame and its colours
  are Firelight's (lib/fire.js).

  A Direction (the fire dying back to the point) would only change
  catchAt() and burn(): the front is the same distance over time, read
  the other way.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.name        = "Fire Spread";
    effect.description = "The stage catches fire from the look's Aim target and the fire spreads out until everything burns";
    effect.author      = "qlcconsole";
    effect.requires    = ["fire"];
    effect.targetDistance = true;   // fixture.fromTarget, inputs._fromTargetFar
    effect.cueTime        = true;   // inputs._cueTime, for Spread over: The cue's time
    effect.naturalDurationMs = 8000;
    effect.notes = "The head nearest the look's Aim target catches fire first and the fire spreads outward across the floor at an even speed until every head burns, then keeps flickering like Firelight for as long as the look plays. Add an Aim look for the point it starts from; without one it starts at centre stage. Spread Time is how long the fire takes to reach the farthest head (or take the cue's time). Edge makes the front ragged; Flare is the flash as a head catches. Heads are dark until the fire reaches them. Needs the rig placed in the 2D plan: heads all in one place catch together.";
    effect.fixtureTypes = [
        "rgb", "rgbw", "rgba", "rgbwa", "rgbwauv",
        "wash", "spot", "beam", "dimmer", "color", "colorwheel",
        "moving", "mover", "movers"
    ];

    effect.inputs = [];
    effect.palettes = [];

    effect.parameters = [
        { name: "spreadOver", label: "Spread Over", description: "What times the spread: the Spread Time below, or the cue's time (the look's Length, else its cue or chase step)",
          defaultValue: 0, values: ["Spread Time", "The Cue's Time"] },
        { name: "spreadTime", label: "Spread Time", description: "Seconds for the fire to reach the farthest head",
          type: "float", min: 0.5, max: 120, defaultValue: 8,
          activeWhen: [{ param: "spreadOver", is: [0] }] },
        { name: "edge",      label: "Edge",      description: "How ragged the fire's front is: 0 a hard line, 1 heads catching early and late at random",
          type: "float", min: 0, max: 1, defaultValue: 0.3 },
        { name: "flare",     label: "Flare",     description: "How bright and white a head flares as it catches",
          type: "float", min: 0, max: 1, defaultValue: 0.6 },
        { name: "speed",     label: "Flicker Speed", description: "Flicker speed multiplier, as Firelight's", min: 0.1, max: 5.0, defaultValue: 1.0 },
        { name: "intensity", description: "Overall brightness (0-1)",    min: 0.0, max: 1.0, defaultValue: 1.0 },
        { name: "coolness",  description: "Add blue (0=fire, 1=ice)",    min: 0.0, max: 1.0, defaultValue: 0.0 },
        { name: "seed",      label: "Seed",      description: "Random seed (same seed, same fire)", type: "float", min: 1, max: 999, defaultValue: 1 }
    ];

    var FLARE_S = 1.0;      // a catch's flare settles in this long
    var ATTACK_S = 0.06;    // ...after snapping up in this long
    var KINDLE_S = 0.6;     // at Edge 1, a head kindles this long before it catches
    var JITTER = 0.25;      // at Edge 1, a head catches up to this much of the spread early or late
    var HOT = { r: 255, g: 235, b: 170 };   // the flare's colour
    var ORANGE = { r: 255, g: 60, b: 0 };   // a colour wheel's one fire slot

    // Deterministic hash -> 0..1.
    function hash(a, b) {
        var s = Math.sin(a * 127.1 + b * 311.7) * 43758.5453;
        return s - Math.floor(s);
    }

    function num(v, d) { return v !== undefined ? v : d; }

    /* When head i catches, in seconds from the start: its share of the
       distance to the farthest head, nudged by the Edge, never after the
       spread is over. */
    function catchAt(d, far, i, seed, spread, edge) {
        var frac = far > 1e-6 ? d / far : 0;
        if (edge > 0)
            frac += edge * JITTER * (2 * hash(i + 1, seed) - 1);
        return Math.max(0, Math.min(1, frac)) * spread;
    }

    /* What a head shows, burning at @p level (0..1 of the look) in colour
       @p c (Firelight's, or hotter while it flares). The way it is said
       depends on the head:
         a head of its own (a par, a mover, a dimmer): colour plus a dimmer
           level, as Firelight; one with no dimmer channel carries the level
           in its colour; a colour-wheel head keeps one slot (orange) and
           flickers on its dimmer, so the wheel never chases;
         a pixel of a pixel bar (one of several heads): the level in its
           colour, and the bar's own dimmer at full while it is lit -- one
           pixel's dimmer would be the whole bar's. */
    function burn(f, c, level, firelight) {
        var o;
        if (f.heads > 1) {
            if (f.headRGB === false && !f.headWhite)
                return { dimmer: level };                       // a dimmer pixel
            var m = firelight ? 1 : level;                      // Firelight's colour already flickers
            if (f.headRGB === false)
                o = { w: Math.round(Math.max(c.r, c.g, c.b) * m) };   // a white pixel
            else
                o = { r: Math.round(c.r * m), g: Math.round(c.g * m), b: Math.round(c.b * m) };
            if (f.hasDimmer && level > 0)
                o.dimmer = 1;
            return o;
        }
        if (f.hasColorWheel && !f.hasRGB)
            return { r: ORANGE.r, g: ORANGE.g, b: ORANGE.b, dimmer: level };
        if (f.hasDimmer) {
            o = { r: c.r, g: c.g, b: c.b, dimmer: level };
            return o;
        }
        // Colour only: the level is in the colour (Firelight's colour as it
        // is once it burns); all black would leave the head to the look.
        var k = firelight ? 1 : level;
        o = { r: Math.round(c.r * k), g: Math.round(c.g * k), b: Math.round(c.b * k) };
        if (o.r === 0 && o.g === 0 && o.b === 0)
            return { dimmer: 0 };
        return o;
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = num(inputs._time, 0);
        var speed  = num(params.speed, 1.0);
        var inten  = num(params.intensity, 1.0);
        var cool   = num(params.coolness, 0.0);
        var edge   = Math.max(0, Math.min(1, num(params.edge, 0.3)));
        var flareK = Math.max(0, Math.min(1, num(params.flare, 0.6)));
        var seed   = Math.floor(num(params.seed, 1));
        var spread = num(params.spreadOver, 0) >= 0.5 ? num(inputs._cueTime, 8)
                                                      : num(params.spreadTime, 8);
        spread = Math.max(0.05, spread);
        var far    = num(inputs._fromTargetFar, 0);
        var kindle = edge * KINDLE_S;
        var fseed  = seed * 13.7;   // the flames' own, so the seed moves them too
        var ember  = FIRE.colour(0.1, cool, inten);

        return fixtures.map(function(f, i) {
            var at = catchAt(num(f.fromTarget, 0), far, i, seed, spread, edge);
            var age = t - at;

            if (age < 0) {
                // Not reached: dark. Just before, with an Edge, it kindles.
                if (kindle <= 0 || age < -kindle)
                    /* A head of its own: dark. A pixel with only a level:
                       dark too, by its own dimmer. A colour pixel: left to
                       what is under it (black would hand it back to the
                       look anyway, and one pixel's master dimmer is the
                       whole bar's). */
                    return f.heads > 1 ? (f.headRGB === false && !f.headWhite ? { dimmer: 0 } : {})
                                       : { dimmer: 0 };
                var k = 1 + age / kindle;                       // 0..1 up to the catch
                var glow = 0.3 * k * FIRE.flame(t * 3, i * 1.618 + fseed + 5, speed);
                return burn(f, ember, glow * inten, false);
            }

            var flame = FIRE.flame(t, i * 1.618 + fseed, speed);
            var c = FIRE.colour(flame, cool, inten);
            var level = flame * inten;
            if (age >= FLARE_S || flareK <= 0)
                return burn(f, c, level, true);                 // burning: Firelight

            // The catch: snap up, flare hot, settle into the flame.
            var w = flareK * Math.pow(1 - age / FLARE_S, 2);
            var hot = { r: Math.round(c.r + (HOT.r * inten - c.r) * w),
                        g: Math.round(c.g + (HOT.g * inten - c.g) * w),
                        b: Math.round(c.b + (HOT.b * inten - c.b) * w) };
            level = level + (inten - level) * w;
            if (age < ATTACK_S) {
                var a = age / ATTACK_S;
                return burn(f, hot, level * a, false);
            }
            return burn(f, hot, level, true);
        });
    };

    return effect;
})()
