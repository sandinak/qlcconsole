/*
  QLC+ Effect Script: UV Pulse
  Phase-shifted pulse on the UV channel across fixtures. Great for UV wash
  effects, black-light looks, and UV-reactive prop highlights.
  Emits only the `uv` intent; other channels follow the look's palettes.
*/
(function() {
    var effect = new Object;
    effect.name        = "UV Pulse";
    effect.description = "Phase-shifted UV channel pulse / chase across fixtures";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["rgbwauv", "uvonly"];

    effect.parameters = [
        { name: "rate",     description: "Rate (Hz)",   type: "float", min: 0.05, max: 10,  defaultValue: 1    },
        { name: "spread",   description: "Spread (°)",  type: "float", min: 0,    max: 360, defaultValue: 360  },
        { name: "waveform", description: "Waveform",    type: "enum",  values: ["Sine","Triangle","Square","Sawtooth Up","Sawtooth Down"], defaultValue: 0 },
        { name: "minUV",    description: "Min UV",      type: "float", min: 0,    max: 255, defaultValue: 0    },
        { name: "maxUV",    description: "Max UV",      type: "float", min: 0,    max: 255, defaultValue: 255  }
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
        var rate   = params.rate     !== undefined ? params.rate     : 1;
        var spread = (params.spread  !== undefined ? params.spread   : 360) / 360;
        var wf     = params.waveform !== undefined ? params.waveform : 0;
        var minUV  = params.minUV    !== undefined ? params.minUV    : 0;
        var maxUV  = params.maxUV    !== undefined ? params.maxUV    : 255;
        var t      = inputs._time    !== undefined ? inputs._time    : 0;

        return fixtures.map(function(f, i) {
            var phase = (t * rate + i * spread / Math.max(n, 1)) % 1;
            var v     = wave(wf, phase);
            return { uv: Math.round(minUV + v * (maxUV - minUV)) };
        });
    };

    return effect;
})();
