/*
  QLC+ Effect Script: Spell Spread
  A colour arrives, light by light. It spreads out from the look's Aim
  target (centre stage when the look has none) -- the spell sweeping the
  stage from the witch, the steps lighting as the cast is bespelled -- or
  sweeps in from one side of the stage: From Upstage it runs downstage,
  down the steps, as blood running down them. Once a light is reached it
  holds the look's colour for as long as the look plays.

  The front is Fire Spread's (lib/spread.js): a head's share of the way
  (its distance from the target over the farthest head's, or how far in
  it stands from the side) is how far through the Spread time it is
  reached, each pixel of a pixel bar by its own place. Edge makes the
  front ragged, each head a little early or late (a seeded hash), with a
  faint glow of the colour just before it arrives. Flash is a bright,
  whiter edge as it arrives, settling into the colour.

  A light the front hasn't reached is left alone: it keeps what the cue
  under it gives (nothing is written for it). Reached, it takes the look's
  colour -- the effect's own Colour, else the look's -- at the look's
  level: a head with a dimmer its dimmer, a pixel bar's pixels in their
  colour (the bar's one dimmer at full while any of it is lit), a colour
  wheel the nearest slot (never flashed: the wheel would chase). Moving
  heads get colour and dimmer only. Lights with no colour (a dimmer, a
  relay) are not its: they keep what is under them.

  The spread is the opening, timed from the effect's start (inputs._time),
  not a cycle: run on Loop it holds the colour and never starts again;
  started again after it has stopped, it spreads again from the start.

  Pure & testable: no Math.random, and what it keeps in state (when each
  light is reached, what each shows once it is) is worked out again
  whenever the heads, the spread or the colour change -- the same time
  gives the same frame.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.name        = "Spell Spread";
    effect.description = "The look's colour spreads out from its Aim target, or sweeps in from a side of the stage, reaching each light in turn";
    effect.author      = "qlcconsole";
    effect.requires    = ["spread"];
    effect.targetDistance = true;   // fixture.fromTarget, inputs._fromTargetFar
    effect.floorPlace     = true;   // fixture.floorX / floorY, inputs._floorBox: From a side
    effect.cueTime        = true;   // inputs._cueTime, for Spread over: The cue's time
    effect.naturalDurationMs = 8000;
    effect.notes = "The look's colour reaches each light in turn: out from the look's Aim target (centre stage without one), or in from a side of the stage (From Upstage runs downstage, down the steps). A light keeps what the cue under it gives until the spell reaches it, then holds the colour for as long as the look plays. Give the effect its own colour (or use the look's). Spread Time is how long it takes to reach the last light (or take the cue's time). Edge makes the front ragged, with a faint glow just before; Flash is a bright edge as it arrives. Needs the rig placed in the 2D plan.";
    effect.fixtureTypes = [
        "rgb", "rgbw", "rgba", "rgbwa", "rgbwauv",
        "wash", "color", "colorwheel",
        "moving", "mover", "movers"
    ];

    effect.inputs = [];
    effect.palettes = [];

    effect.parameters = [
        { name: "from", label: "From", description: "Where the spell starts: the look's Aim target (centre stage without one), or a side of the stage it sweeps in from (Upstage runs downstage, down the steps)",
          defaultValue: 0, values: ["The Aim Target", "Upstage", "Downstage", "Stage Left", "Stage Right"] },
        { name: "spreadOver", label: "Spread Over", description: "What times the spread: the Spread Time below, or the cue's time (the look's Length, else its cue or chase step)",
          defaultValue: 0, values: ["Spread Time", "The Cue's Time"] },
        { name: "spreadTime", label: "Spread Time", description: "Seconds for the spell to reach the last light",
          type: "float", min: 0.5, max: 120, defaultValue: 6,
          activeWhen: [{ param: "spreadOver", is: [0] }] },
        { name: "edge",      label: "Edge",      description: "How ragged the front is: 0 a hard line, 1 lights reached early and late at random, each glowing faintly just before",
          type: "float", min: 0, max: 1, defaultValue: 0.2 },
        { name: "flash",     label: "Flash",     description: "How bright and white a light flashes as the spell reaches it",
          type: "float", min: 0, max: 1, defaultValue: 0.5 },
        { name: "seed",      label: "Seed",      description: "Random seed for a ragged edge (same seed, same front)", type: "float", min: 1, max: 999, defaultValue: 1 }
    ];

    var FLASH_S = 0.8;      // a flash settles into the colour in this long
    var LEAD_S = 0.6;       // at Edge 1, a light glows this long before it is reached
    var GLOW = 0.3;         // ...up to this much of the look's level
    var WHITE = { r: 255, g: 255, b: 255 };

    var num = SPREAD.num;

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = num(inputs._time, 0);
        var L      = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var c      = (L[0] && L[0].r !== undefined) ? L[0] : WHITE;
        var edge   = Math.max(0, Math.min(1, num(params.edge, 0.2)));
        var flash  = Math.max(0, Math.min(1, num(params.flash, 0.5)));
        var seed   = Math.floor(num(params.seed, 1));
        var spread = SPREAD.seconds(params, inputs);
        var from   = Math.round(num(params.from, 0));
        var lead   = edge * LEAD_S;
        var settle = flash > 0 ? FLASH_S : 0;          // reached this long: just the colour
        var top    = Math.max(c.r, c.g, c.b);
        var n      = fixtures.length;

        /* Kept from tick to tick (a rig of thousands of pixels): when each
           light is reached, until the heads or the spread change; what each
           shows once it is reached, until they or the colour do. Once every
           light has it, every tick is that same frame. */
        var A = SPREAD.arrivals(fixtures, inputs, from, seed, spread, edge, state);
        var held = state ? state._held : undefined;
        if (held === undefined || held.places !== A || held.r !== c.r || held.g !== c.g || held.b !== c.b) {
            var frame = new Array(n), relay = new Int8Array(n);
            for (var h = 0; h < n; h++) {
                relay[h] = fixtures[h].switches ? 1 : 0;
                frame[h] = relay[h] ? {} : SPREAD.light(fixtures[h], c, 1, false, c);
            }
            held = { places: A, r: c.r, g: c.g, b: c.b, frame: frame, relay: relay };
            if (state)
                state._held = held;
        }
        if (t >= A.last + settle)
            return held.frame;

        var at = A.at, frameNow = held.frame, relay = held.relay, none = {}, out = new Array(n);
        for (var i = 0; i < n; i++) {
            var age = t - at[i];
            if (relay[i] === 1) {
                out[i] = none;                  // a relay has no colour: not a spell's
                continue;
            }
            if (age >= settle) {
                out[i] = frameNow[i];           // reached: the colour
                continue;
            }
            var f = fixtures[i];
            if (age < 0) {
                // Not reached: what is under it. Just before, with an Edge,
                // a faint glow of the colour.
                var glow = lead > 0 && age > -lead ? GLOW * (1 + age / lead) : 0;
                out[i] = glow * top < 1 ? none : SPREAD.light(f, c, glow, false, c);
                continue;
            }
            // Arriving: whiter, settling into the colour.
            var w = flash * Math.pow(1 - age / FLASH_S, 2);
            var hot = { r: Math.round(c.r + (WHITE.r - c.r) * w),
                        g: Math.round(c.g + (WHITE.g - c.g) * w),
                        b: Math.round(c.b + (WHITE.b - c.b) * w) };
            out[i] = SPREAD.light(f, hot, 1, false, c);
        }
        return out;
    };

    return effect;
})()
