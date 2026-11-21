/*
  QLC+ Effect Script: Lightning Spread
  Lightning from one point. Two ways to play it (Mode):

  One Burst: a single blast. A front races out from the look's Aim target
  (centre stage when the look has none) in every direction at once, as
  Fire Spread's front does but in a fraction of a second; each head it
  hits flashes to full and then flickers out the way lightning does -- a
  sharp strike, dark, a weaker re-strike or two at uneven gaps, gone --
  over the Flicker Out time. Then it is at rest: it writes nothing, and
  each head shows what is under it again (dark over black).

  Static Generator: it keeps going, a static generator's arcs. The heads
  nearest the point never go dark (a jagged glow, Core Glow), and every
  so often, at uneven gaps (Rate a second on average), an arc crawls out
  from there through some of the heads -- a different, random few each
  time -- and flickers on each as it passes, weaker as it goes, never
  further than its Reach.

  The front: a head's distance from the point over the farthest head's
  (fixture.fromTarget / inputs._fromTargetFar, each pixel of a pixel bar
  by its own place) is how far through the Burst Time it is hit -- the
  front of lib/spread.js, ragged by the Edge. A generator's Reach and its
  arcs are measured from the nearest head out, so they start where the
  light is however far the point stands from it.

  Every flicker is a seeded hash (no Math.random): each head's strikes in
  a burst, each arc's time, reach and heads. The same seed and time give
  the same frame, and the same burst every run.

  Colours: the look's first colour (the effect's own Colour look under it,
  else the look's), else a cold, slightly blue white. A head with no
  colour (a dimmer) flickers in level; a colour wheel holds its nearest
  slot and flickers on its dimmer; a moving head gets colour and dimmer
  only, so it never moves. A relay is not its (a flicker would chatter
  it): it keeps what is under it.

  Runs: One Burst on Runs: Once (At the end: Hold, Length auto) ends when
  the last head has flickered out (state.done; effect.endsWhenDone), and
  holds that rest. On Loop it bursts once and stays at rest; started
  again, it bursts again. A Static Generator never says it is done.

  Fast: what it works out per head (where each is, its strikes, its kind)
  is kept in state until the heads, their places or the settings change;
  what a kind of head shows at each of 256 levels is kept until the
  colour changes, so a tick allocates nothing but its answer array.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.name        = "Lightning Spread";
    effect.description = "Lightning blasts out from the look's Aim target and flickers out, or crawls out from it like a static generator";
    effect.author      = "qlcconsole";
    effect.requires    = ["spread"];
    effect.targetDistance = true;   // fixture.fromTarget, inputs._fromTargetFar
    effect.naturalDurationMs = 3000;
    effect.endsWhenDone   = true;   // Runs: Once ends when the burst has flickered out (state.done)
    effect.notes = "Lightning from the look's Aim target (centre stage without one). One Burst: a single blast races out in every direction, each head flashes to full as it is hit, then flickers out like lightning -- a strike, a re-strike or two at uneven gaps, gone -- and the stage is left to what is under it. Static Generator: the heads at the centre never go dark, and arcs crawl out from them at uneven gaps through a random few heads at a time, never beyond the Reach. Uses the effect's own Colour, else the look's, else a cold white. A dimmer flickers in level; moving heads get colour and dimmer only. Runs: Once ends a burst when the last head has flickered out. Needs the rig placed in the 2D plan.";
    effect.fixtureTypes = [
        "rgb", "rgbw", "rgba", "rgbwa", "rgbwauv",
        "wash", "spot", "beam", "dimmer", "color", "colorwheel",
        "moving", "mover", "movers"
    ];

    effect.inputs = [];
    effect.palettes = [];

    effect.parameters = [
        { name: "mode",      label: "Mode",      description: "One Burst: a single blast that flickers out. Static Generator: arcs crawl out from the point again and again",
          defaultValue: 0, values: ["One Burst", "Static Generator"] },
        { name: "spreadTime", label: "Burst Time", description: "Seconds for the blast to reach the farthest head",
          type: "float", min: 0.05, max: 10, defaultValue: 0.3,
          activeWhen: [{ param: "mode", is: [0] }] },
        { name: "edge",      label: "Edge",      description: "How jagged the blast's front is: 0 a clean ring, 1 heads hit early and late at random",
          type: "float", min: 0, max: 1, defaultValue: 0.4,
          activeWhen: [{ param: "mode", is: [0] }] },
        { name: "decay",     label: "Flicker Out", description: "Seconds each head flickers before it is out",
          type: "float", min: 0.2, max: 10, defaultValue: 1.5,
          activeWhen: [{ param: "mode", is: [0] }] },
        { name: "strikes",   label: "Strikes",   description: "Most strikes a head flickers with (the first and its re-strikes; some heads get fewer)",
          type: "float", min: 1, max: 6, defaultValue: 3,
          activeWhen: [{ param: "mode", is: [0] }] },
        { name: "rate",      label: "Rate",      description: "Arcs a second, on average (they come at uneven gaps)",
          type: "float", min: 0.2, max: 20, defaultValue: 4,
          activeWhen: [{ param: "mode", is: [1] }] },
        { name: "reach",     label: "Reach",     description: "How far out the arcs go, of the way from the nearest head to the farthest",
          unit: "%", min: 5, max: 100, defaultValue: 60,
          activeWhen: [{ param: "mode", is: [1] }] },
        { name: "crawl",     label: "Crawl Time", description: "Seconds an arc takes to crawl out to the Reach",
          type: "float", min: 0.05, max: 3, defaultValue: 0.3,
          activeWhen: [{ param: "mode", is: [1] }] },
        { name: "core",      label: "Core Glow", description: "How bright the heads at the centre glow between arcs: they never go dark",
          unit: "%", min: 5, max: 100, defaultValue: 25,
          activeWhen: [{ param: "mode", is: [1] }] },
        { name: "lightningIntensity", label: "Intensity", description: "How bright the lightning is: 100% at full, 50% half as bright",
          unit: "%", min: 0, max: 100, defaultValue: 100 },
        { name: "seed",      label: "Seed",      description: "Random seed (same seed, same lightning)", type: "float", min: 1, max: 999, defaultValue: 1 }
    ];

    var COLD = { r: 200, g: 215, b: 255 };  // lightning with no colour of the look's
    var STRIKE_TAU = 0.045;     // a strike dies away this fast (seconds), give or take
    var TAIL = 0.15;            // the last share of Flicker Out fades whatever is left to none
    var ARC_LIFE = 0.3;         // an arc's flicker on a head lasts this long
    var ARC_TAU = 0.035;
    var CORE_STEP = 1 / 25;     // the core's glow jumps this often (jagged, not a sine)
    var CORE_SHARE = 0.2;       // the core: the nearest fifth of the Reach
    var GONE = 6.3;             // e^-6.3 is under half a DMX step: a strike that far gone shows nothing
    var MAX_ARCS = 128;         // arcs alive at once, at most (Rate 20, Crawl 3)
    var ARC_FIELDS = 8;

    var num = SPREAD.num;
    var hash = SPREAD.hash;

    function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /* What SPREAD.light reads of a head, as one number: heads of the same
       kind show a level the same way, so they share its answer. */
    function kindOf(f) {
        if (f.switches)
            return -1;                  // a relay: not lightning's
        return (f.heads > 1 ? 1 : 0) | (f.headRGB === false ? 2 : 0) | (f.headWhite ? 4 : 0)
             | (f.hasDimmer ? 8 : 0) | (f.hasColorWheel ? 16 : 0) | (f.hasRGB ? 32 : 0);
    }

    /* Each head's kind, and the heads' shares of the way from the nearest
       to the farthest (the generator's Reach is of that), in the heads'
       order and nearest first (order, sorted), kept until the heads'
       places (A) change. */
    function heads(fixtures, A, seed, state) {
        var h = state ? state._heads : undefined;
        if (h !== undefined && h.places === A && h.seed === seed)
            return h;
        var n = fixtures.length, kind = new Int8Array(n), out = new Float64Array(n), u = new Float64Array(n);
        var rep = {};
        var lo = Infinity, hi = 0;
        for (var i = 0; i < n; i++) {
            kind[i] = kindOf(fixtures[i]);
            if (kind[i] >= 0 && rep[kind[i]] === undefined)
                rep[kind[i]] = fixtures[i];
            if (A.at[i] < lo) lo = A.at[i];
            if (A.at[i] > hi) hi = A.at[i];
        }
        var span = hi - lo, ids = [];
        for (var j = 0; j < n; j++) {
            out[j] = span > 1e-9 ? (A.at[j] - lo) / span : 0;
            u[j] = hash(j + 1, seed + 0.9);
            ids.push(j);
        }
        ids.sort(function(a, b) { return out[a] - out[b] || a - b; });
        var order = new Int32Array(n), sorted = new Float64Array(n);
        for (var k = 0; k < n; k++) {
            order[k] = ids[k];
            sorted[k] = out[ids[k]];
        }
        h = { places: A, seed: seed, kind: kind, rep: rep, out: out, u: u, order: order, sorted: sorted };
        if (state)
            state._heads = h;
        return h;
    }

    /* What a head of kind @p k shows at level @p q (0..255) in colour @p c:
       worked out once a colour, kept for every head and tick after. */
    function looks(H, c, state) {
        var L = state ? state._looks : undefined;
        if (L === undefined || L.heads !== H || L.r !== c.r || L.g !== c.g || L.b !== c.b) {
            L = { heads: H, r: c.r, g: c.g, b: c.b, table: {} };
            if (state)
                state._looks = L;
        }
        return L;
    }

    function shown(L, H, c, k, q) {
        var t = L.table[k];
        if (t === undefined)
            t = L.table[k] = new Array(256);
        var o = t[q];
        if (o === undefined)
            o = t[q] = SPREAD.light(H.rep[k], c, q / 255, false, c);
        return o;
    }

    /* A burst's strikes on each head: up to @p K, the first at once at
       full, the rest at uneven gaps and weaker, some heads with fewer (a
       seeded hash a head). Kept until the heads, Flicker Out, Strikes or
       the seed change: s (when, after the head is hit), a (how bright),
       tau (how fast it dies), K a head. */
    function strikes(A, decay, K, seed, state) {
        var S = state ? state._strikes : undefined;
        if (S !== undefined && S.places === A && S.decay === decay && S.K === K && S.seed === seed)
            return S;
        var n = A.at.length, s = new Float64Array(n * K), a = new Float64Array(n * K), tau = new Float64Array(n * K);
        var gone = new Float64Array(n * K), end = new Float64Array(n);
        var span = decay * (1 - TAIL);
        for (var i = 0; i < n; i++) {
            var j = i * K, at = 0;
            s[j] = 0;
            a[j] = 1;
            tau[j] = STRIKE_TAU * (1.2 + 1.2 * hash(i + 1, seed + 0.11));
            for (var k = 1; k < K; k++) {
                at += span / K * (0.35 + 1.3 * hash(i + 1, seed + k * 0.17 + 0.3));
                s[j + k] = at;
                // Some heads skip a re-strike: an uneven count, head to head.
                var skip = k > 1 && hash(i + 1, seed + k * 0.23 + 0.7) < 0.3;
                a[j + k] = skip || at >= span ? 0
                         : (0.4 + 0.6 * hash(i + 1, seed + k * 0.29 + 0.5)) * (1 - 0.45 * k / K);
                tau[j + k] = STRIKE_TAU * (0.6 + 1.0 * hash(i + 1, seed + k * 0.31 + 0.9));
            }
            // When each strike, and the head's last, has died below half a
            // DMX step (it would round to 0 at full): past that a tick
            // works nothing out for it.
            for (var g = 0; g < K; g++) {
                gone[j + g] = a[j + g] > 0 ? s[j + g] + GONE * tau[j + g] : -1;
                if (gone[j + g] > end[i])
                    end[i] = gone[j + g];
            }
        }
        S = { places: A, decay: decay, K: K, seed: seed, s: s, a: a, tau: tau, gone: gone, end: end };
        if (state)
            state._strikes = S;
        return S;
    }

    function burst(fixtures, t, params, inputs, state, seed, c, inten) {
        var spread = SPREAD.seconds(params, inputs);
        var edge   = clamp(num(params.edge, 0.4), 0, 1);
        var decay  = clamp(num(params.decay, 1.5), 0.2, 10);
        var K      = Math.round(clamp(num(params.strikes, 3), 1, 6));
        var A = SPREAD.arrivals(fixtures, inputs, 0, seed, spread, edge, state);
        var H = heads(fixtures, A, seed, state);
        var S = strikes(A, decay, K, seed, state);
        var L = looks(H, c, state);
        if (state)
            state.done = t >= A.last + decay;
        var n = fixtures.length, at = A.at, kind = H.kind, ss = S.s, aa = S.a, tt = S.tau, gg = S.gone, ee = S.end;
        var none = state && state._none ? state._none : {};
        if (state)
            state._none = none;
        var tailFrom = decay * (1 - TAIL);
        var out = new Array(n);
        for (var i = 0; i < n; i++) {
            var age = t - at[i];
            var kd = kind[i];
            if (kd < 0 || age < 0 || age >= decay) {
                out[i] = none;                  // not hit yet, or out: what is under it
                continue;
            }
            var lv = 0, j = i * K;
            if (age >= ee[i]) {
                out[i] = shown(L, H, c, kd, 0); // every strike has died away: dark till it is out
                continue;
            }
            for (var k = 0; k < K; k++) {
                var since = age - ss[j + k];
                if (since < 0)
                    break;                      // the strikes are in time order
                if (age >= gg[j + k])
                    continue;                   // this one has died away
                var v = aa[j + k] * Math.exp(-since / tt[j + k]);
                if (v > lv)
                    lv = v;
            }
            if (age > tailFrom)
                lv *= (decay - age) / (decay - tailFrom);
            out[i] = shown(L, H, c, kd, (clamp(lv, 0, 1) * inten * 255 + 0.5) | 0);
        }
        return out;
    }

    /* The generator's arcs alive at @p t, into G.arcs (kept in state):
       each slot of 1/Rate seconds has an arc at an uneven time within it
       (most slots do), with its own reach, re-strike and share of the
       heads it runs through. Returns how many. */
    function arcsAt(G, t, rate, reach, crawl, seed) {
        var slot = 1 / rate;
        var first = Math.floor((t - crawl - ARC_LIFE) / slot) - 1;
        var last = Math.floor(t / slot);
        if (first < 0)
            first = 0;
        if (last - first + 1 > MAX_ARCS)
            first = last - MAX_ARCS + 1;
        var m = 0, arcs = G.arcs;
        for (var k = first; k <= last; k++) {
            if (hash(k + 1, seed + 0.25) >= 0.85)
                continue;                       // a quiet slot
            var j = m * ARC_FIELDS;
            arcs[j]     = (k + 0.8 * hash(k + 1, seed + 0.5)) * slot;           // when
            arcs[j + 1] = reach * (0.35 + 0.65 * hash(k + 1, seed + 0.75));     // how far
            arcs[j + 2] = hash(k + 1, seed + 0.125);                            // which heads
            arcs[j + 3] = 0.3 + 0.4 * hash(k + 1, seed + 0.375);                // ...how many
            arcs[j + 4] = 0.05 + 0.08 * hash(k + 1, seed + 0.625);              // its re-strike
            arcs[j + 5] = 0.35 + 0.5 * hash(k + 1, seed + 0.875);
            m++;
        }
        return m;
    }

    /* The first of the nearest-first @p sorted shares above @p d. */
    function firstAbove(sorted, d) {
        var lo = 0, hi = sorted.length;
        while (lo < hi) {
            var mid = (lo + hi) >> 1;
            if (sorted[mid] <= d)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    }

    function generator(fixtures, t, params, inputs, state, seed, c, inten) {
        var rate  = clamp(num(params.rate, 4), 0.2, 20);
        var reach = clamp(num(params.reach, 60), 5, 100) / 100;
        var crawl = clamp(num(params.crawl, 0.3), 0.05, 3);
        var core  = clamp(num(params.core, 25), 5, 100) / 100;
        var A = SPREAD.arrivals(fixtures, inputs, 0, seed, 1, 0, state);
        var H = heads(fixtures, A, seed, state);
        var L = looks(H, c, state);
        var n = fixtures.length, kind = H.kind, d = H.out, u = H.u, order = H.order, sorted = H.sorted;
        if (state)
            state.done = false;
        var G = state ? state._gen : undefined;
        if (G === undefined || G.lv.length !== n) {
            G = { arcs: new Float64Array(MAX_ARCS * ARC_FIELDS), lv: new Float64Array(n) };
            if (state)
                state._gen = G;
        }
        var lv = G.lv;
        lv.fill(0);

        // The centre: never dark, its glow jumping CORE_STEP by CORE_STEP.
        var coreW = Math.max(0.05, CORE_SHARE * reach);
        var g = hash(Math.floor(t / CORE_STEP) + 1, seed + 0.4);
        for (var k = 0; k < n && sorted[k] < coreW; k++) {
            var ic = order[k], x = u[ic] * 7.13 + g;
            x -= Math.floor(x);
            lv[ic] = core * (1 - sorted[k] / coreW) * (0.55 + 0.45 * x);
        }

        // Each arc alive: only the heads it is crossing now (a ring of
        // shares, nearest first), and of those its own.
        var m = arcsAt(G, t, rate, reach, crawl, seed);
        var arcs = G.arcs, kk = crawl / reach;
        for (var a = 0; a < m; a++) {
            var j = a * ARC_FIELDS;
            var base = t - arcs[j], r = arcs[j + 1];
            var dHi = Math.min(r, base / kk), dLo = (base - ARC_LIFE) / kk;
            if (dHi < 0 || dLo > r)
                continue;
            var pick = arcs[j + 2], share = arcs[j + 3], s2 = arcs[j + 4], a2 = arcs[j + 5];
            for (var q = dLo < 0 ? 0 : firstAbove(sorted, dLo); q < n; q++) {
                var di = sorted[q];
                if (di > dHi)
                    break;
                var i = order[q];
                var w = u[i] - pick;
                if (w < 0)
                    w += 1;
                if (w >= share && di > 1e-9)
                    continue;                   // not one of its heads (the nearest always are)
                var age = base - di * kk;
                var v = Math.exp(-age / ARC_TAU);
                var since = age - s2;
                if (since >= 0) {
                    var v2 = a2 * Math.exp(-since / ARC_TAU);
                    if (v2 > v)
                        v = v2;
                }
                v *= 1 - 0.5 * di / r;
                if (v > lv[i])
                    lv[i] = v;
            }
        }

        var none = state && state._none ? state._none : {};
        if (state)
            state._none = none;
        var out = new Array(n);
        for (var h = 0; h < n; h++) {
            var kd = kind[h], l = lv[h], isCore = d[h] < coreW;
            if (kd < 0 || (l <= 0 && !isCore)) {
                out[h] = none;
                continue;
            }
            var lq = ((l > 1 ? 1 : l) * inten * 255 + 0.5) | 0;
            if (isCore && lq < 1 && inten > 0)
                lq = 1;
            out[h] = shown(L, H, c, kd, lq);
        }
        return out;
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t    = num(inputs._time, 0);
        var seed = Math.floor(num(params.seed, 1));
        var Lc   = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var c    = (Lc[0] && Lc[0].r !== undefined) ? Lc[0] : COLD;
        var look = palettes ? palettes.look : undefined;
        var own  = look ? look.ownDimmer : undefined;
        var inten = clamp(num(params.lightningIntensity, 100), 0, 100) / 100
                  * (own === undefined ? 1 : Math.max(0, Math.min(1, own)));
        if (num(params.mode, 0) >= 0.5)
            return generator(fixtures, t, params, inputs, state, seed, c, inten);
        return burst(fixtures, t, params, inputs, state, seed, c, inten);
    };

    return effect;
})()
