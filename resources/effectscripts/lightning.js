/*
  QLC+ Effect Script: Lightning
  Occasional brilliant white strikes — calm between flashes, then a
  multi-burst electrical storm.  Each strike hits a random subset of
  fixtures for realism.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Lightning";
    effect.description = "Random electrical storm flashes across the rig";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "rgb"];
    effect.notes = "Simulates electrical storm lightning. Flashes arrive in random multi-burst clusters with quiet periods in between. Coverage controls what fraction of the rig is hit per strike. flashDur sets individual flash length in seconds. Bind a colour palette for non-white lightning (e.g. red storm, alien blue).";

    effect.inputs = [];

    effect.palettes = [
        { name: "color", type: "Color", optional: true }
    ];

    effect.parameters = [
        { name: "rate",      description: "Average strikes per second",  min: 0.1, max: 10.0, defaultValue: 1.0  },
        { name: "burstLen",  description: "Max burst sub-flashes",       min: 1.0, max: 6.0,  defaultValue: 3.0  },
        { name: "flashDur",  description: "Single flash duration (sec)", min: 0.01, max: 0.3, defaultValue: 0.06 },
        { name: "coverage",  description: "Fraction of fixtures hit",    min: 0.1, max: 1.0,  defaultValue: 0.6  },
        { name: "dimmer",    description: "Peak dimmer",                 min: 0.0, max: 1.0,  defaultValue: 1.0  }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var dt       = 0.02;
        var t        = inputs._time !== undefined ? inputs._time : 0;

        var rate     = params.rate     !== undefined ? params.rate     : 1.0;
        var burstLen = params.burstLen !== undefined ? params.burstLen : 3.0;
        var flashDur = params.flashDur !== undefined ? params.flashDur : 0.06;
        var coverage = params.coverage !== undefined ? params.coverage : 0.6;
        var dimmer   = params.dimmer   !== undefined ? params.dimmer   : 1.0;

        var fc = (palettes.color && palettes.color.r !== undefined)
                 ? palettes.color : { r: 220, g: 220, b: 255 };

        // State: { flashes: [{endTime, mask[]}], nextStrike }
        if (!state.flashes)   state.flashes    = [];
        if (!state.nextStrike) state.nextStrike = t + Math.random() / rate;

        // Trigger a new burst?
        if (t >= state.nextStrike) {
            var bursts = Math.ceil(1 + Math.random() * (burstLen - 1));
            for (var b = 0; b < bursts; b++) {
                var offset = b * (flashDur * 1.5 + Math.random() * 0.04);
                var mask = [];
                for (var fi = 0; fi < fixtures.length; fi++)
                    mask[fi] = (Math.random() < coverage) ? 1 : 0;
                state.flashes.push({ end: t + offset + flashDur, start: t + offset, mask: mask });
            }
            state.nextStrike = t + (0.5 + Math.random()) / rate;
        }

        // Build output: any active flash illuminates its masked fixtures
        var lit = [];
        for (var i = 0; i < fixtures.length; i++) lit[i] = 0;

        // Purge expired flashes
        var active = [];
        for (var fi = 0; fi < state.flashes.length; fi++) {
            var fl = state.flashes[fi];
            if (t >= fl.start && t <= fl.end) {
                for (var i = 0; i < fixtures.length; i++)
                    if (fl.mask[i]) lit[i] = 1;
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
