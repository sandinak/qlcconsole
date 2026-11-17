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

  Before the front a head is left to what is under it: the layers below,
  or the look's own values (board Q259: it casts over what is there). Over
  black, as the show it was made for plays it, that is dark until the
  front arrives. Where the fire is, it lays over what is there. Moving
  heads get colour and dimmer only. A relay (a silk flame's fan) switches
  on as it catches and stays on: its flicker would chatter it.

  Intensity (0-100 %) scales the fire's own light, every head but a relay
  (on or off), as the effect's own Dimmer does (ownLevel()); 0 % leaves a
  relay off too. A look saved before it with the old 0-1 "intensity"
  still plays at that level (read, not shown).

  Run Once (the look's Runs), with Length auto, it ends when the fire has
  reached every head and settled (state.done; effect.endsWhenDone) -- not
  at the cue's time -- and At the end: Hold keeps that last frame until
  the cue is released, stopped or replaced.

  Pure & testable: no Math.random, and what it keeps in state (when each
  head catches, each head's flame phases) is worked out again whenever the
  heads, the spread or the seed change -- the same time gives the same
  fire. The flame and its colours are Firelight's (lib/fire.js; worked out
  for every head at once, its colours in 256 steps), the front and how
  each kind of head shows it are shared with Spell Spread (lib/spread.js).

  Colours: Front and Behind. The look's first colour (the effect's own
  Colour, else the look's) is the front: a head snaps to it as the front
  reaches it, with the Flare. A second front follows from the same point,
  Front Width seconds behind each head, and with an Edge ragged again in
  its own seeded way (up to half the width either side, so the front's
  colour always shows); as it reaches a head the head
  crossfades over BLEND_S into the second colour, the backfill, and burns
  there with the fire's flicker in level (Flicker Behind; off, steady).
  With only one colour the backfill is the fire itself; with none, or on
  a head with no colour (a dimmer, a white pixel), it is Fire as ever. A
  colour wheel holds the nearest slot to each colour, never blended. When
  the second front reaches each head is worked out with the first, and
  kept in state with it.

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
    effect.endsWhenDone   = true;   // Runs: Once ends when every head burns (state.done)
    effect.notes = "The head nearest the look's Aim target catches fire first and the fire spreads outward across the floor at an even speed until every head burns, then keeps flickering like Firelight for as long as the look plays. Add an Aim look for the point it starts from; without one it starts at centre stage. Spread Time is how long the fire takes to reach the farthest head (or take the cue's time). Edge makes the front ragged; Flare is the flash as a head catches. A light the fire hasn't reached keeps what is under it (dark over black); a relay (a silk flame) switches on as it catches. Colours: Front and Behind makes the look's first colour the front and its second the backfill a Front Width behind (one colour: fire behind the front). Intensity scales the fire's own light. Runs: Once, At the end: Hold keeps the fire as it stands once it has reached everything. Needs the rig placed in the 2D plan: heads all in one place catch together.";
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
        { name: "colours",   label: "Colours",   description: "Fire: the fire's own colours. Front and Behind: the look's first colour (the effect's own Colour, else the look's) is the front, its second the backfill behind it (one colour: fire behind)",
          defaultValue: 0, values: ["Fire", "Front and Behind"] },
        { name: "frontWidth", label: "Front Width", description: "Seconds the front's colour holds on a head before the backfill reaches it",
          type: "float", min: 0.1, max: 20, defaultValue: 1.5,
          activeWhen: [{ param: "colours", is: [1] }] },
        { name: "flickerBehind", label: "Flicker Behind", description: "The backfill flickers in level like the fire; off, it is steady (a fire behind always flickers)",
          defaultValue: 1, values: ["Off", "On"],
          activeWhen: [{ param: "colours", is: [1] }] },
        { name: "speed",     label: "Flicker Speed", description: "Flicker speed multiplier, as Firelight's", min: 0.1, max: 5.0, defaultValue: 1.0 },
        { name: "fireIntensity", label: "Intensity", description: "How bright the fire is: 100% as it burns, 50% half as bright. Only the fire's own light: a light it hasn't reached keeps what is under it",
          unit: "%", min: 0, max: 100, defaultValue: 100 },
        { name: "coolness",  description: "Add blue (0=fire, 1=ice)",    min: 0.0, max: 1.0, defaultValue: 0.0 },
        { name: "seed",      label: "Seed",      description: "Random seed (same seed, same fire)", type: "float", min: 1, max: 999, defaultValue: 1 }
    ];

    var FLARE_S = 1.0;      // a catch's flare settles in this long
    var ATTACK_S = 0.06;    // ...after snapping up in this long
    var KINDLE_S = 0.6;     // at Edge 1, a head kindles this long before it catches
    var HOT = { r: 255, g: 235, b: 170 };   // the flare's colour
    var ORANGE = { r: 255, g: 60, b: 0 };   // a colour wheel's one fire slot
    var BLEND_S = 0.4;      // Front and Behind: the backfill crossfades in over this long
    var BACK_JITTER = 0.5;  // ...and at Edge 1 reaches a head up to half the width early or late

    var num = SPREAD.num;
    // The kinds of head the tick tells apart. LEVEL (no colour: a dimmer, a
    // white or dimmer pixel) and WHEEL (a colour wheel, no mixing) burn as
    // OTHER in Fire; only Front and Behind tells them apart.
    var OTHER = 0, PIXEL = 1, RELAY = 2, LEVEL = 3, WHEEL = 4;

    function colourAt(L, i) {
        var c = L[i];
        return (c && c.r !== undefined) ? c : null;
    }

    /* Two intents of one head mixed, @p x of the way from @p a to @p b:
       the colours and the levels. A head that shows black as { dimmer: 0 }
       is black in colour. A colour wheel's slot is never mixed (the wheel
       would chase): it is @p a's until half way, then @p b's. */
    function mix(a, b, x, wheel) {
        var o = {}, any = false;
        if (wheel) {
            var w = x < 0.5 ? a : b;
            o.r = w.r; o.g = w.g; o.b = w.b;
        } else {
            var keys = ["r", "g", "b", "w"];
            for (var k = 0; k < 4; k++) {
                var key = keys[k];
                if (a[key] === undefined && b[key] === undefined)
                    continue;
                var va = a[key] === undefined ? 0 : a[key];
                var vb = b[key] === undefined ? 0 : b[key];
                o[key] = Math.round(va + (vb - va) * x);
                any = any || o[key] > 0;
            }
        }
        var da = a.dimmer, db = b.dimmer;
        var aBlack = da === 0 && a.r === undefined && a.w === undefined;
        var bBlack = db === 0 && b.r === undefined && b.w === undefined;
        if (da !== undefined && db !== undefined)
            o.dimmer = da + (db - da) * x;
        else if (da !== undefined && !aBlack)
            o.dimmer = da;
        else if (db !== undefined && !bBlack)
            o.dimmer = db;
        if (o.dimmer === undefined && !any)
            return { dimmer: 0 };
        return o;
    }

    /* What a head shows, burning at @p level (0..1 of the look) in colour
       @p c (Firelight's, or hotter while it flares): SPREAD.light, a colour
       wheel on its one orange slot. */
    function burn(f, c, level, firelight) {
        return SPREAD.light(f, c, level, firelight, ORANGE);
    }

    /* When the backfill reaches each head: Front Width after the front
       did, so it follows the front's own ragged line from the same point,
       and with an Edge a little early or late of its own (up to half the
       width at Edge 1; a seeded hash, so repeatable): never so early the
       front's colour doesn't show. Kept in state until the first front's
       arrivals or the width change: { at, last }, last the latest. */
    function backfillArrivals(A, seed, edge, width, state) {
        var b = state ? state._behind : undefined;
        if (b !== undefined && b.places === A && b.width === width)
            return b;
        var n = A.at.length, at = new Float64Array(n), last = 0;
        var seed2 = seed + 0.5;         // its own hash: the user's seeds are whole
        for (var i = 0; i < n; i++) {
            at[i] = A.at[i] + width * (1 + edge * BACK_JITTER * (2 * SPREAD.hash(i + 1, seed2) - 1));
            if (at[i] > last)
                last = at[i];
        }
        b = { places: A, width: width, at: at, last: last };
        if (state)
            state._behind = b;
        return b;
    }

    /* Colours: Front and Behind. Each head: dark (kindling in the front's
       colour) before the front; the front's colour c1 with the Flare once
       it is reached; then, as the backfill reaches it, a BLEND_S crossfade
       into c2 burning with the flicker in level (or steady), or, with no
       c2, into the fire itself. A head with no colour (LEVEL) is Fire's. */
    function frontAndBehind(fixtures, inputs, params, state, A, bank, k, table, shared, on, o) {
        var t = o.t, inten = o.inten, flareK = o.flareK, c1 = o.c1, c2 = o.c2;
        var width = Math.max(0.1, Math.min(20, num(params.frontWidth, 1.5)));
        var flicker = c2 === null || num(params.flickerBehind, 1) >= 0.5;
        var B = backfillArrivals(A, o.seed, o.edge, width, state);
        var at = A.at, at2 = B.at;
        // Done (a run of Once ends here) once the backfill has reached every head.
        if (state && t < B.last + BLEND_S)
            state.done = false;
        var ph = bank.ph, kinds = bank.kind, n = fixtures.length;
        var k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];
        var steadyQ = 255;                      // Flicker Behind off: the backfill at full
        var behindPx = new Array(256);          // a pixel's backfill, one object a flame step
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var age = t - at[i];
            var kd = kinds[i];
            if (kd === RELAY) {
                out[i] = age >= 0 ? on : shared.none;
                continue;
            }
            var f = fixtures[i];
            if (age < 0) {
                if (o.kindle <= 0 || age < -o.kindle) {
                    out[i] = shared.none;               // left to what is under it
                    continue;
                }
                var kk = 1 + age / o.kindle;
                var glow = 0.3 * kk * FIRE.flame(t * 3, i * 1.618 + o.fseed + 5, o.speed);
                out[i] = kd === LEVEL ? burn(f, FIRE.colour(0.1, o.cool, inten), glow * inten, false)
                                      : SPREAD.light(f, c1, glow * inten, false, c1);
                continue;
            }

            // The flame, as Fire's: the backfill's flicker, and the fire.
            var j = i * 6;
            var flame = 0.5 + 0.25 * (k0 * ph[j + 1] + k1 * ph[j])
                            + 0.15 * (k2 * ph[j + 3] + k3 * ph[j + 2])
                            + 0.10 * (k4 * ph[j + 5] + k5 * ph[j + 4]);
            flame = flame < 0 ? 0 : (flame > 1 ? 1 : flame);
            var q = (flame * 255 + 0.5) | 0;
            var c = table[q];
            if (c === undefined)
                c = table[q] = FIRE.colour(q / 255, o.cool, inten);
            var w = (age < FLARE_S && flareK > 0) ? flareK * Math.pow(1 - age / FLARE_S, 2) : 0;

            if (kd === LEVEL) {
                // No colour to show: Fire's, its flare in level.
                var lv = flame * inten;
                lv = lv + (inten - lv) * w;
                out[i] = burn(f, c, age < ATTACK_S ? lv * (age / ATTACK_S) : lv, age >= ATTACK_S);
                continue;
            }

            var age2 = t - at2[i];
            var behind = null;
            if (age2 >= 0) {
                // The backfill: c2 with the flicker in level, or the fire.
                if (c2 === null)
                    behind = kd === PIXEL ? c : burn(f, c, flame * inten, true);
                else {
                    var bq = flicker ? q : steadyQ;
                    if (kd === PIXEL) {
                        behind = behindPx[bq];
                        if (behind === undefined)
                            behind = behindPx[bq] = SPREAD.light(f, c2, bq / 255 * inten, false, c2);
                    } else
                        behind = SPREAD.light(f, c2, bq / 255 * inten, false, c2);
                }
                if (age2 >= BLEND_S) {
                    out[i] = behind;
                    continue;
                }
            }

            // The front: c1 at full, flaring hot as it catches.
            var fc = w > 0 ? { r: Math.round(c1.r + (HOT.r - c1.r) * w),
                               g: Math.round(c1.g + (HOT.g - c1.g) * w),
                               b: Math.round(c1.b + (HOT.b - c1.b) * w) } : c1;
            var front = SPREAD.light(f, fc, age < ATTACK_S ? inten * (age / ATTACK_S) : inten, false, c1);
            out[i] = behind === null ? front : mix(front, behind, age2 / BLEND_S, kd === WHEEL);
        }
        return out;
    }

    function fireTick(fixtures, inputs, palettes, params, state) {
        var t      = num(inputs._time, 0);
        var speed  = num(params.speed, 1.0);
        var inten  = num(params.intensity, 1.0);   // a look saved before Intensity (0-1)
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
                        : fs.heads > 1 ? (fs.headRGB === false ? LEVEL : (!fs.hasDimmer ? PIXEL : OTHER))
                        : (fs.hasColorWheel && !fs.hasRGB) ? WHEEL
                        : !fs.hasRGB ? LEVEL : OTHER;
            }
            bank = { n: n, fseed: fseed, places: A, ph: FIRE.phases(seeds), kind: kind };
            if (state)
                state._flames = bank;
        }
        var ph = bank.ph, kinds = bank.kind;
        /* Done (effect.endsWhenDone: a run of Once ends here, and At the
           end: Hold keeps this frame) once every head has caught and its
           flare settled; Front and Behind waits for the backfill too. */
        if (state)
            state.done = t >= A.last + (flareK > 0 ? FLARE_S : ATTACK_S);
        var k = FIRE.clock(t, speed);
        var k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5];
        var table = new Array(256);     // this tick's colours, one object a flame step
        var shared = { none: {}, off: { dimmer: 0 } };
        var on = { dimmer: inten > 0 && num(params.fireIntensity, 100) > 0 ? 1 : 0 };
        if (state)
            state._on = on;                 // a relay's: the effect's own Dimmer leaves it be

        // Colours: Front and Behind, with a colour to be the front. Without
        // one it is Fire, as ever (the loop below).
        if (num(params.colours, 0) >= 0.5) {
            var L = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
            var c1 = colourAt(L, 0);
            if (c1 !== null)
                return frontAndBehind(fixtures, inputs, params, state, A, bank, k, table, shared, on,
                                      { t: t, speed: speed, inten: inten, cool: cool, flareK: flareK,
                                        seed: seed, spread: spread, edge: edge, kindle: kindle,
                                        fseed: fseed, c1: c1, c2: colourAt(L, 1) });
        }

        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var age = t - at[i];
            var kd = kinds[i];

            // A relay: off until the fire catches, then on, and it stays on.
            if (kd === RELAY) {
                out[i] = age >= 0 ? on : shared.none;
                continue;
            }

            if (age < 0) {
                // Not reached: left to what is under it (Q259: dark over
                // black). Just before, with an Edge, it kindles.
                if (kindle <= 0 || age < -kindle) {
                    out[i] = shared.none;
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
    }

    /* Intensity, and the effect's own Dimmer look (one under it in the
       look's list): their level, on top of the fire's. The look's own Dimmer (above the
       effects) the host applies itself. A head that shows its level in its
       colour takes it as a dimmer (the host scales its colour by it), a
       head with a dimmer has its dimmer scaled; a relay stays as it is
       (between on and off it would chatter). Only with a Dimmer of its own:
       otherwise every head is what fireTick gave, object for object. */
    function ownLevel(out, level, on) {
        for (var i = 0; i < out.length; i++) {
            var o = out[i];
            if (o === on || o === undefined)
                continue;
            var d = o.dimmer;
            if (d === 0)
                continue;
            if (d === undefined && o.r === undefined && o.w === undefined)
                continue;                       // left to what is under it
            var c = {};
            for (var key in o)
                c[key] = o[key];
            c.dimmer = (d === undefined ? 1 : d) * level;
            out[i] = c;
        }
        return out;
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var out = fireTick(fixtures, inputs, palettes, params, state);
        var look = palettes ? palettes.look : undefined;
        var own = look ? look.ownDimmer : undefined;
        var level = (own === undefined ? 1 : Math.max(0, own))
                  * Math.max(0, Math.min(100, num(params.fireIntensity, 100))) / 100;
        if (level >= 1)
            return out;
        return ownLevel(out, level, state ? state._on : undefined);
    };

    return effect;
})()
