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
  of the look's level" -- on a pixel bar with no dimmer, pixel by pixel; on
  one whose single dimmer serves every pixel, a pixel not yet reached is
  left to what is under it. Moving heads get colour and dimmer only. A
  relay (a silk flame's fan) switches on as it catches and stays on: its
  flicker would chatter it.

  Pure & testable: no Math.random, and what it keeps in state (when each
  head catches, each head's flame phases) is worked out again whenever the
  heads, the spread or the seed change -- the same time gives the same
  fire. The flame and its colours are Firelight's (lib/fire.js; worked out
  for every head at once, its colours in 256 steps), the front and how
  each kind of head shows it are shared with Spell Spread (lib/spread.js).

  A Direction (the fire dying back to the point) would only change when
  a head is reached (SPREAD.front): the front is the same distance over
  time, read the other way.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.name        = "Fire Spread";
    effect.description = "The stage catches fire from the look's Aim target and the fire spreads out until everything burns";
    effect.author      = "qlcconsole";
    effect.requires    = ["fire", "spread"];
    effect.targetDistance = true;   // fixture.fromTarget, inputs._fromTargetFar
    effect.cueTime        = true;   // inputs._cueTime, for Spread over: The cue's time
    effect.naturalDurationMs = 8000;
    effect.notes = "The head nearest the look's Aim target catches fire first and the fire spreads outward across the floor at an even speed until every head burns, then keeps flickering like Firelight for as long as the look plays. Add an Aim look for the point it starts from; without one it starts at centre stage. Spread Time is how long the fire takes to reach the farthest head (or take the cue's time). Edge makes the front ragged; Flare is the flash as a head catches. Heads are dark until the fire reaches them; a relay (a silk flame) switches on as it catches. Needs the rig placed in the 2D plan: heads all in one place catch together.";
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
    var HOT = { r: 255, g: 235, b: 170 };   // the flare's colour
    var ORANGE = { r: 255, g: 60, b: 0 };   // a colour wheel's one fire slot

    var num = SPREAD.num;
    var OTHER = 0, PIXEL = 1, RELAY = 2;    // the kinds of head the tick tells apart

    /* What a head shows, burning at @p level (0..1 of the look) in colour
       @p c (Firelight's, or hotter while it flares): SPREAD.light, a colour
       wheel on its one orange slot. */
    function burn(f, c, level, firelight) {
        return SPREAD.light(f, c, level, firelight, ORANGE);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = num(inputs._time, 0);
        var speed  = num(params.speed, 1.0);
        var inten  = num(params.intensity, 1.0);
        var cool   = num(params.coolness, 0.0);
        var edge   = Math.max(0, Math.min(1, num(params.edge, 0.3)));
        var flareK = Math.max(0, Math.min(1, num(params.flare, 0.6)));
        var seed   = Math.floor(num(params.seed, 1));
        var spread = SPREAD.seconds(params, inputs);
        var kindle = edge * KINDLE_S;
        var fseed  = seed * 13.7;   // the flames' own, so the seed moves them too
        var ember  = FIRE.colour(0.1, cool, inten);
        var n      = fixtures.length;

        /* Kept from tick to tick (a rig of thousands of pixels): when each
           head catches, until the heads or the spread change; what kind of
           head each is, until the heads do; each head's flame phases, until
           the heads or the seed do. The loop below reads them, not the
           heads: it runs for every pixel of the rig, every tick. */
        var A = SPREAD.arrivals(fixtures, inputs, 0, seed, spread, edge, state);
        var at = A.at;
        var bank = state ? state._flames : undefined;
        if (bank === undefined || bank.n !== n || bank.fseed !== fseed || bank.places !== A) {
            var seeds = new Float64Array(n), kind = new Int8Array(n);
            for (var s = 0; s < n; s++) {
                var fs = fixtures[s];
                seeds[s] = s * 1.618 + fseed;
                kind[s] = fs.switches ? RELAY
                        : (fs.heads > 1 && fs.headRGB !== false && !fs.hasDimmer) ? PIXEL : OTHER;
            }
            bank = { n: n, fseed: fseed, places: A, ph: FIRE.phases(seeds), kind: kind };
            if (state)
                state._flames = bank;
        }
        var ph = bank.ph, kinds = bank.kind;
        var k = FIRE.clock(t, speed);
        var k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];
        var table = new Array(256);     // this tick's colours, one object a flame step
        var shared = { none: {}, off: { dimmer: 0 } };
        var on = { dimmer: inten > 0 ? 1 : 0 };

        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var age = t - at[i];
            var kd = kinds[i];

            // A relay: off until the fire catches, then on, and it stays on.
            if (kd === RELAY) {
                out[i] = age >= 0 ? on : shared.off;
                continue;
            }

            if (age < 0) {
                // Not reached: dark. Just before, with an Edge, it kindles.
                if (kindle <= 0 || age < -kindle) {
                    out[i] = SPREAD.dark(fixtures[i], shared);
                    continue;
                }
                var kk = 1 + age / kindle;                      // 0..1 up to the catch
                var glow = 0.3 * kk * FIRE.flame(t * 3, i * 1.618 + fseed + 5, speed);
                out[i] = burn(fixtures[i], ember, glow * inten, false);
                continue;
            }

            // Firelight's flame (FIRE.flameAt, written out) and its colour
            // in 256 steps (FIRE.colour, each step worked out once a tick).
            var j = i * 6;
            var flame = 0.5 + 0.25 * (k0 * ph[j + 1] + k1 * ph[j])
                            + 0.15 * (k2 * ph[j + 3] + k3 * ph[j + 2])
                            + 0.10 * (k4 * ph[j + 5] + k5 * ph[j + 4]);
            flame = flame < 0 ? 0 : (flame > 1 ? 1 : flame);
            var q = (flame * 255 + 0.5) | 0;
            var c = table[q];
            if (c === undefined)
                c = table[q] = FIRE.colour(q / 255, cool, inten);
            if (age >= FLARE_S || flareK <= 0) {
                // Burning: Firelight. A pixel with no dimmer is its colour,
                // as it is: the one object for every pixel at that flame.
                out[i] = kd === PIXEL ? c : burn(fixtures[i], c, flame * inten, true);
                continue;
            }

            // The catch: snap up, flare hot, settle into the flame.
            var level = flame * inten;
            var w = flareK * Math.pow(1 - age / FLARE_S, 2);
            var hot = { r: Math.round(c.r + (HOT.r * inten - c.r) * w),
                        g: Math.round(c.g + (HOT.g * inten - c.g) * w),
                        b: Math.round(c.b + (HOT.b * inten - c.b) * w) };
            level = level + (inten - level) * w;
            if (age < ATTACK_S)
                out[i] = burn(fixtures[i], hot, level * (age / ATTACK_S), false);
            else
                out[i] = burn(fixtures[i], hot, level, true);
        }
        return out;
    };

    return effect;
})()
