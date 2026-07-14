/*
  QLC+ Effect Script: Amber Pulse
  Phase-shifted pulse on the Amber channel across fixtures. On RGBA/RGBWA
  fixtures, amber adds warmth and richness without shifting hue as aggressively
  as orange from R+G. Great for candle flicker, sunset glow, and fire looks.
  Emits only the `a` intent; RGB/dimmer channels follow the look's palettes.
*/
(function() {
    var effect = new Object;
    effect.name        = "Amber Pulse";
    effect.description = "Phase-shifted amber channel pulse / chase across fixtures";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["rgba", "rgbwa", "rgbwauv"];

    effect.parameters = [
        { name: "rate",      description: "Rate (Hz)",       type: "float", min: 0.05, max: 10,  defaultValue: 0.8  },
        { name: "spread",    description: "Spread (°)",      type: "float", min: 0,    max: 360, defaultValue: 180  },
        { name: "waveform",  description: "Waveform",        type: "enum",  values: ["Sine","Triangle","Square","Sawtooth Up","Sawtooth Down"], defaultValue: 0 },
        { name: "variation", description: "Variation (0=steady,1=candle-random)", type: "float", min: 0, max: 1, defaultValue: 0 },
        { name: "minA",      description: "Min Amber",       type: "float", min: 0,    max: 255, defaultValue: 0    },
        { name: "maxA",      description: "Max Amber",       type: "float", min: 0,    max: 255, defaultValue: 255  }
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
        var rate   = params.rate   !== undefined ? params.rate   : 0.8;
        var spread = (params.spread !== undefined ? params.spread : 180) / 360;
        var wf     = params.waveform !== undefined ? params.waveform : 0;
        var vary   = params.variation !== undefined ? params.variation : 0;
        var minA   = params.minA   !== undefined ? params.minA   : 0;
        var maxA   = params.maxA   !== undefined ? params.maxA   : 255;
        var t      = inputs._time  !== undefined ? inputs._time  : 0;

        return fixtures.map(function(f, i) {
            var phase = (t * rate + i * spread / Math.max(n, 1)) % 1;
            var v     = wave(wf, phase);
            // Variation layers two incommensurable sines (per-fixture phase) for
            // organic, non-repeating candle-like wander on top of the base wave.
            if (vary > 0) {
                var w = 0.5
                    + 0.30 * Math.sin(2 * Math.PI * (t * rate * 2.376 + i * 0.37))
                    + 0.20 * Math.sin(2 * Math.PI * (t * rate * 5.831 + i * 0.71));
                v = v * (1 - vary) + Math.max(0, Math.min(1, w)) * vary;
            }
            return { a: Math.round(minA + v * (maxA - minA)) };
        });
    };

    return effect;
})();
