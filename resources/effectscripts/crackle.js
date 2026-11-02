/*
  QLC+ Effect Script: Crackle
  Intensity-ONLY. Fixtures sit at a quiet rest level and, at random moments,
  one pulses UP -- a fast attack -- in its own look colour, then fades back to
  rest. Unlike Sparkle (which flashes and fades like stars), Crackle keeps the
  look's colour and returns to the look's level.

  Like Candle Flicker it emits `dimmer` as a 0..1 *multiplier* on each
  fixture's look level (the host never lets a multiplier go above the look),
  so the look's own level is the pulse PEAK and `rest` is the level between
  pulses: a Dimmer look at 100% rests at `rest` and crackles up to 100%.
  Colour is never written, so every pulse is the look's own colour.

  Pure & testable: no Math.random and no state. A pulse's start is a hash of
  the fixture index, a time slot and the seed, and each tick looks back over
  the slots a pulse could still be running from -- the same seed and time
  always give the same crackle, in the harness and in the app.
*/
(function() {
    var effect = new Object;
    effect.name        = "Crackle";
    effect.description = "Fixtures pulse up at random in their own colour and fade back to rest (intensity only)";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.author      = "qlcconsole";
    effect.notes = "Each fixture rests at a quiet level and, at random moments, pulses up to the look's own level in the look's own colour, then fades back. Rate is pulses a second across the whole group; Decay sets how long each takes to fade, so a long decay lets several overlap. Set the look's Dimmer to the peak you want. Unlike Sparkle, the colour stays the look's and the fixture returns to rest, not to dark.";
    effect.fixtureTypes = [
        "rgb", "rgbw", "rgba", "rgbwa", "rgbwauv",
        "wash", "spot", "beam", "dimmer", "color",
        "moving", "mover", "movers"
    ];

    effect.parameters = [
        { name: "rate",   description: "Pulses per second across the group", type: "float", min: 0.1, max: 30,  defaultValue: 3 },
        { name: "rest",   description: "Level between pulses (x the look's level)", type: "float", min: 0, max: 1, defaultValue: 0.35 },
        { name: "peak",   description: "Pulse level (x the look's level)",   type: "float", min: 0, max: 1,  defaultValue: 1.0 },
        { name: "attack", description: "Snap up time (s)",                   type: "float", min: 0.005, max: 0.5, defaultValue: 0.03 },
        { name: "decay",  description: "Fade back time (s)",                 type: "float", min: 0.05, max: 5, defaultValue: 0.7 },
        { name: "seed",   description: "Random seed (same seed, same crackle)", type: "float", min: 1, max: 999, defaultValue: 1 }
    ];

    // Deterministic hash -> 0..1 from integer keys.
    function hash(a, b, c) {
        var s = Math.sin(a * 127.1 + b * 311.7 + c * 74.7) * 43758.5453;
        return s - Math.floor(s);
    }

    function num(v, d) { return v !== undefined ? v : d; }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = num(inputs._time, 0);
        var rate   = Math.max(0.01, num(params.rate, 3));
        var rest   = Math.max(0, Math.min(1, num(params.rest, 0.35)));
        var peak   = Math.max(rest, Math.min(1, num(params.peak, 1.0)));
        var attack = Math.max(0.001, num(params.attack, 0.03));
        var decay  = Math.max(0.01, num(params.decay, 0.7));
        var seed   = Math.floor(num(params.seed, 1));
        var n      = Math.max(1, fixtures.length);

        var win  = attack + decay;                 // how long one pulse lasts
        var slot = Math.max(0.05, win / 6);        // a pulse starts at most once a slot
        var prob = Math.min(1, rate * slot / n);   // per fixture, per slot
        var last = Math.floor(t / slot);
        var first = Math.max(0, Math.floor((t - win) / slot));

        return fixtures.map(function(f, i) {
            var env = 0;
            for (var k = first; k <= last; k++) {
                if (hash(i + 1, k, seed) >= prob) continue;
                var start = (k + hash(i + 7, k, seed + 31)) * slot;
                var age = t - start;
                if (age < 0 || age >= win) continue;
                var e = age < attack ? age / attack
                                     : Math.pow(1 - (age - attack) / decay, 2);
                if (e > env) env = e;
            }
            return { dimmer: rest + (peak - rest) * env };
        });
    };

    return effect;
})()
