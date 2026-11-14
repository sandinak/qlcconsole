/*
  QLC+ Effect Script library: Spread

  What the spreading effects share (Fire Spread, Spell Spread): a front
  that reaches each light in turn, and how each kind of light shows what
  the front brings.

  The front: a head's share of the way -- its distance from the look's Aim
  target over the farthest head's (fixture.fromTarget,
  inputs._fromTargetFar: effect.targetDistance), or how far in it stands
  from one side of the stage (fixture.floorX / floorY over
  inputs._floorBox: effect.floorPlace) -- is how far through the spread it
  is reached, so a head twice as far is reached twice as late. The Edge
  makes the front ragged: each head a little early or late, by a seeded
  hash, so the same seed is the same front every time.

  An effect says
      effect.requires = ["spread"];
  and, each tick,
      var spread = SPREAD.seconds(params, inputs);       // its length
      var reach  = SPREAD.front(inputs, from);           // head -> 0..1
      ... fixtures.map(function(f, i) {
          var at = SPREAD.arrival(reach(f), i, seed, spread, edge);
          ... t < at: not reached; SPREAD.light(f, colour, level, ...)
      });

  No state, no Math.random: the same time gives the same frame.
*/
var SPREAD = (function() {
    var S = {};

    /** At Edge 1, a head is reached up to this much of the spread early
     *  or late. */
    S.JITTER = 0.25;

    /** Deterministic hash -> 0..1. */
    S.hash = function(a, b) {
        var s = Math.sin(a * 127.1 + b * 311.7) * 43758.5453;
        return s - Math.floor(s);
    };

    S.num = function(v, d) { return v !== undefined ? v : d; };

    /** The spread's length in seconds: Spread Time, or (Spread Over: The
     *  Cue's Time) the cue's, as the host hands it in. */
    S.seconds = function(params, inputs) {
        var s = S.num(params.spreadOver, 0) >= 0.5 ? S.num(inputs._cueTime, 8)
                                                   : S.num(params.spreadTime, 8);
        return Math.max(0.05, s);
    };

    /** How far through the spread each head is reached, 0..1, before the
     *  Edge: a function of the head. @p from, as an effect's From setting
     *  lists them: 0 out from the Aim target (centre stage without one);
     *  1 Upstage (the front runs downstage), 2 Downstage, 3 Stage Left,
     *  4 Stage Right (+X is stage left, +Y downstage). Everything in one
     *  place: all at once. */
    S.front = function(inputs, from) {
        if (from >= 1 && from <= 4) {
            var b = inputs._floorBox;
            if (b === undefined)
                return function() { return 0; };
            var deep = from <= 2;                       // up- or downstage: along Y
            var lo = deep ? b.y0 : b.x0;
            var span = deep ? b.y1 - b.y0 : b.x1 - b.x0;
            if (!(span > 1e-6))
                return function() { return 0; };
            if (from === 1)
                return function(f) { return (S.num(f.floorY, lo) - lo) / span; };
            if (from === 2)
                return function(f) { return 1 - (S.num(f.floorY, lo) - lo) / span; };
            if (from === 3)
                return function(f) { return 1 - (S.num(f.floorX, lo) - lo) / span; };
            return function(f) { return (S.num(f.floorX, lo) - lo) / span; };
        }
        var far = S.num(inputs._fromTargetFar, 0);
        if (!(far > 1e-6))
            return function() { return 0; };
        return function(f) { return S.num(f.fromTarget, 0) / far; };
    };

    /** When head @p i is reached, in seconds from the start: @p frac of the
     *  spread, nudged by the Edge, never before the start or after the end. */
    S.arrival = function(frac, i, seed, spread, edge) {
        if (edge > 0)
            frac += edge * S.JITTER * (2 * S.hash(i + 1, seed) - 1);
        return Math.max(0, Math.min(1, frac)) * spread;
    };

    /** When each head is reached: { at, last } -- at[i] in seconds from
     *  the start (S.arrival() of S.front(), a Float64Array in the heads'
     *  order), last the latest. Worked out again only when the heads'
     *  places (inputs._placesBuilt), how many there are, @p from, @p seed,
     *  @p spread or @p edge change: kept in @p state between ticks, not
     *  redone a head a tick. */
    S.arrivals = function(fixtures, inputs, from, seed, spread, edge, state) {
        var n = fixtures.length, built = inputs._placesBuilt;
        var c = state ? state._spreadAt : undefined;
        if (c !== undefined && built !== undefined && c.built === built && c.n === n
            && c.from === from && c.seed === seed && c.spread === spread && c.edge === edge)
            return c;
        var reach = S.front(inputs, from);
        var at = new Float64Array(n), last = 0;
        for (var i = 0; i < n; i++) {
            at[i] = S.arrival(reach(fixtures[i]), i, seed, spread, edge);
            if (at[i] > last)
                last = at[i];
        }
        c = { built: built, n: n, from: from, seed: seed, spread: spread, edge: edge,
              at: at, last: last };
        if (state && built !== undefined)
            state._spreadAt = c;
        return c;
    };

    /** What a head shows, lit at @p level (0..1 of the look) in colour @p c.
       @p levelled: the colour already carries the level (a flicker coloured
       by its own brightness), so a head that shows its level in its colour
       takes it as it is. @p wheel: the colour a colour-wheel head (one with
       no colour mixing) holds, so the wheel never chases. The way it is
       said depends on the head:
         a head of its own (a par, a mover, a dimmer): colour plus a dimmer
           level; one with no dimmer channel carries the level in its
           colour; a colour-wheel head holds @p wheel and goes up and down
           on its dimmer;
         a pixel of a pixel bar (one of several heads): the level in its
           colour, and the bar's own dimmer at full while it is lit -- one
           pixel's dimmer would be the whole bar's. */
    S.light = function(f, c, level, levelled, wheel) {
        var o;
        if (f.heads > 1) {
            if (f.headRGB === false && !f.headWhite)
                return { dimmer: level };                       // a dimmer pixel
            var m = levelled ? 1 : level;
            if (f.headRGB === false)
                o = { w: Math.round(Math.max(c.r, c.g, c.b) * m) };   // a white pixel
            else
                o = { r: Math.round(c.r * m), g: Math.round(c.g * m), b: Math.round(c.b * m) };
            if (f.hasDimmer && level > 0)
                o.dimmer = 1;
            else if (!f.hasDimmer && !o.r && !o.g && !o.b && !o.w)
                return { dimmer: 0 };       // black hands it back to the look: dark is none of it
            return o;
        }
        if (f.hasColorWheel && !f.hasRGB)
            return { r: wheel.r, g: wheel.g, b: wheel.b, dimmer: level };
        if (f.hasDimmer)
            return { r: c.r, g: c.g, b: c.b, dimmer: level };
        // Colour only: the level is in the colour. All black would leave
        // the head to the look: dark is its colour scaled to none.
        var k = levelled ? 1 : level;
        o = { r: Math.round(c.r * k), g: Math.round(c.g * k), b: Math.round(c.b * k) };
        if (o.r === 0 && o.g === 0 && o.b === 0)
            return { dimmer: 0 };
        return o;
    };

    /** A head held dark (the front hasn't reached it, and the effect is
       dark before it): its own dimmer, or its colour scaled to none. A
       pixel whose bar has one dimmer for every pixel is left to what is
       under it -- dark by that dimmer would be the whole bar dark.
       @p shared, when given, is { none: {}, off: { dimmer: 0 } }: the
       answer is one of those, the same object for every head (read only). */
    S.dark = function(f, shared) {
        var leave = f.heads > 1 && f.hasDimmer && (f.headRGB !== false || f.headWhite);
        if (shared)
            return leave ? shared.none : shared.off;
        return leave ? {} : { dimmer: 0 };
    };

    return S;
})();
