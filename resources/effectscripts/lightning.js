/*
  QLC+ Effect Script: Lightning
  Occasional brilliant white strikes — calm between flashes, then a
  multi-burst electrical storm.  Each strike hits a deterministic subset
  of fixtures for realism.

  Pure & testable: there is no Math.random() or wall-clock — all "randomness"
  is a hash of the strike index and the fixture index, and time comes from
  inputs._time.  The same inputs always yield the same storm, so the script
  runs identically in the harness and in the app.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Lightning";
    effect.description = "Random electrical storm flashes across the rig";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "rgb"];
    effect.notes = "Simulates electrical storm lightning. Flashes arrive in pseudo-random multi-burst clusters with quiet periods in between. Coverage controls what fraction of the rig is hit per strike. flashDur sets individual flash length in seconds. Bind a colour palette for non-white lightning (e.g. red storm, alien blue).";

    effect.inputs = [];

    // Bolt colour comes from the LOOK's first Colour palette.

    effect.parameters = [
        { name: "rate",      description: "Average strikes per second",  min: 0.1, max: 10.0, defaultValue: 1.0  },
        { name: "burstLen",  description: "Max burst sub-flashes",       min: 1.0, max: 6.0,  defaultValue: 3.0  },
        { name: "flashDur",  description: "Single flash duration (sec)", min: 0.01, max: 0.3, defaultValue: 0.06 },
        { name: "coverage",  description: "Fraction of fixtures hit",    min: 0.1, max: 1.0,  defaultValue: 0.6  },
        { name: "dimmer",    description: "Peak dimmer",                 min: 0.0, max: 1.0,  defaultValue: 1.0  }
    ];

    // Deterministic hash → 0..1, seeded from integer keys (no Math.random).
    // Cheap sine-hash: stable across ticks, well-mixed for storm timing/masks.
    function hash(a, b) {
        var s = Math.sin(a * 127.1 + b * 311.7) * 43758.5453;
        return s - Math.floor(s);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t        = inputs._time !== undefined ? inputs._time : 0;

        var rate     = params.rate     !== undefined ? params.rate     : 1.0;
        var burstLen = params.burstLen !== undefined ? params.burstLen : 3.0;
        var flashDur = params.flashDur !== undefined ? params.flashDur : 0.06;
        var coverage = params.coverage !== undefined ? params.coverage : 0.6;
        var dimmer   = params.dimmer   !== undefined ? params.dimmer   : 1.0;

        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var fc = (L[0] && L[0].r !== undefined) ? L[0] : { r: 220, g: 220, b: 255 };

        // State carries only the strike scheduler:
        //   strikeIdx  – monotonic index of the next strike to schedule
        //   nextStrike – time of that strike
        //   flashes    – [{ start, end, mask[] }] currently pending/active
        if (state.strikeIdx === undefined) {
            state.strikeIdx  = 0;
            state.nextStrike = t + hash(0, 0.5) / rate;
            state.flashes    = [];
        }

        // Trigger any strikes whose scheduled time has arrived. Each strike is a
        // burst of sub-flashes; both the burst size and per-fixture masks are
        // hashed from the strike index so the storm is reproducible.
        var guard = 0;
        while (t >= state.nextStrike && guard++ < 64) {
            var k      = state.strikeIdx;
            var bursts = Math.ceil(1 + hash(k, 1.3) * (burstLen - 1));
            for (var b = 0; b < bursts; b++) {
                var offset = b * (flashDur * 1.5 + hash(k, b + 2.7) * 0.04);
                var mask = [];
                for (var fi = 0; fi < fixtures.length; fi++)
                    mask[fi] = (hash(k * 13.0 + fi, b + 5.1) < coverage) ? 1 : 0;
                state.flashes.push({
                    start: state.nextStrike + offset,
                    end:   state.nextStrike + offset + flashDur,
                    mask:  mask
                });
            }
            state.strikeIdx  = k + 1;
            state.nextStrike = state.nextStrike + (0.5 + hash(k, 9.2)) / rate;
        }

        // Build output: any active flash illuminates its masked fixtures.
        var lit = [];
        for (var i = 0; i < fixtures.length; i++) lit[i] = 0;

        // Keep pending/active flashes; purge expired ones.
        var active = [];
        for (var j = 0; j < state.flashes.length; j++) {
            var fl = state.flashes[j];
            if (t >= fl.start && t <= fl.end) {
                for (var m = 0; m < fixtures.length; m++)
                    if (fl.mask[m]) lit[m] = 1;
                active.push(fl);
            } else if (t < fl.start) {
                active.push(fl);  // keep future flashes
            }
        }
        state.flashes = active;

        return fixtures.map(function(f, i) {
            var b = lit[i] * dimmer;
            var intent = {};
            if (f.hasDimmer) intent.dimmer = b;
            intent.r = Math.round(fc.r * b);
            intent.g = Math.round(fc.g * b);
            intent.b = Math.round(fc.b * b);
            return intent;
        });
    };

    return effect;
})()
