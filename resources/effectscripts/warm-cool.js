/*
  QLC+ Effect Script: Warm Cool
  Oscillates between warm white (W channel) and cool blue (B channel) across
  fixtures — a colour-temperature wave. Works on RGBW / RGBWAUV fixtures.
  Phase-shifts across the array to create a traveling temperature sweep.
*/
(function() {
    var effect = new Object;
    effect.name        = "Warm Cool";
    effect.description = "Colour-temperature oscillator — alternates W (warm) and B (cool) across fixtures";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["rgbw", "rgbwa", "rgbwauv"];

    effect.parameters = [
        { name: "rate",     description: "Rate (Hz)",       type: "float", min: 0.05, max: 10,  defaultValue: 0.5  },
        { name: "spread",   description: "Spread (°)",      type: "float", min: 0,    max: 360, defaultValue: 180  },
        { name: "waveform", description: "Waveform",        type: "enum",  values: ["Sine","Triangle","Square","Sawtooth Up","Sawtooth Down"], defaultValue: 0 },
        { name: "minW",     description: "Min Warm",        type: "float", min: 0,    max: 255, defaultValue: 0    },
        { name: "maxW",     description: "Max Warm",        type: "float", min: 0,    max: 255, defaultValue: 200  },
        { name: "minC",     description: "Min Cool (blue)", type: "float", min: 0,    max: 255, defaultValue: 0    },
        { name: "maxC",     description: "Max Cool (blue)", type: "float", min: 0,    max: 255, defaultValue: 180  }
    ];

    function wave(wf, t) {
        t = (t % 1 + 1) % 1;
        switch (Math.round(wf)) {
            case 1: return t < 0.5 ? 2*t : 2*(1-t);
            case 2: return t < 0.5 ? 1 : 0;
            case 3: return t;
            case 4: return 1 - t;
            default: return 0.5 + 0.5 * Math.sin(2 * Math.PI * t);
        }
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n      = fixtures.length;
        var rate   = params.rate     !== undefined ? params.rate     : 0.5;
        var spread = (params.spread  !== undefined ? params.spread   : 180) / 360;
        var wf     = params.waveform !== undefined ? params.waveform : 0;
        var minW   = params.minW     !== undefined ? params.minW     : 0;
        var maxW   = params.maxW     !== undefined ? params.maxW     : 200;
        var minC   = params.minC     !== undefined ? params.minC     : 0;
        var maxC   = params.maxC     !== undefined ? params.maxC     : 180;
        var t      = inputs._time    !== undefined ? inputs._time    : 0;

        return fixtures.map(function(f, i) {
            var phase = (t * rate + i * spread / Math.max(n, 1)) % 1;
            var v     = wave(wf, phase);
            return { w: Math.round(minW + v * (maxW - minW)),
                     b: Math.round(minC + (1 - v) * (maxC - minC)) };
        });
    };

    return effect;
})();
