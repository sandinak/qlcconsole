/*
  QLC+ Effect Script: Iris Pulse
  Pulses the iris / aperture across fixtures — closed to open and back.
  Great for iris pops, rhythmic aperture effects, and textured beams.
  Emits the `iris` intent (0 = closed, 1 = fully open).
*/
(function() {
    var effect = new Object;
    effect.name        = "Iris Pulse";
    effect.description = "Iris / aperture pulse / chase — closed to open";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["moving", "spot", "beam"];

    effect.parameters = [
        { name: "rate",     description: "Rate (Hz)",           type: "float", min: 0.05, max: 10, defaultValue: 1   },
        { name: "spread",   description: "Spread (°)",          type: "float", min: 0,    max: 360,defaultValue: 180 },
        { name: "waveform", description: "Waveform",            type: "enum",  values: ["Sine","Triangle","Square","Sawtooth Up","Sawtooth Down"], defaultValue: 0 },
        { name: "minIris",  description: "Min iris (0=closed)", type: "float", min: 0,    max: 1,  defaultValue: 0   },
        { name: "maxIris",  description: "Max iris (1=open)",   type: "float", min: 0,    max: 1,  defaultValue: 1   }
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
        var n    = fixtures.length;
        var rate = params.rate     !== undefined ? params.rate     : 1;
        var sprd = (params.spread  !== undefined ? params.spread   : 180) / 360;
        var wf   = params.waveform !== undefined ? params.waveform : 0;
        var minI = params.minIris  !== undefined ? params.minIris  : 0;
        var maxI = params.maxIris  !== undefined ? params.maxIris  : 1;
        var t    = inputs._time    !== undefined ? inputs._time    : 0;

        return fixtures.map(function(f, i) {
            var phase = (t * rate + i * sprd / Math.max(n, 1)) % 1;
            return { iris: minI + wave(wf, phase) * (maxI - minI) };
        });
    };

    return effect;
})();
