/*
  QLC+ Effect Script: Native Strobe
  Drives the hardware shutter channel to strobe at a controlled rate.
  Uses the shutterOpen intent so the host maps it to the fixture's real
  shutter/strobe channel rather than toggling the dimmer.
*/
(function() {
    var effect = new Object;
    effect.name        = "Native Strobe";
    effect.description = "Hardware shutter strobe via the shutterOpen intent";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["moving", "spot", "wash", "beam"];

    effect.parameters = [
        { name: "rate",       description: "Strobe rate (Hz)",     type: "float", min: 0.5,  max: 25,   defaultValue: 5   },
        { name: "spread",     description: "Phase spread (°)",     type: "float", min: 0,    max: 360,  defaultValue: 0   },
        { name: "duty",       description: "Duty cycle (0–1)",     type: "float", min: 0.05, max: 0.95, defaultValue: 0.5 },
        { name: "strobeRate", description: "Strobe speed (0–1)",   type: "float", min: 0,    max: 1,    defaultValue: 0.5 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n          = fixtures.length;
        var rate       = params.rate       !== undefined ? params.rate       : 5;
        var spread     = (params.spread    !== undefined ? params.spread     : 0) / 360;
        var duty       = params.duty       !== undefined ? params.duty       : 0.5;
        var strobeRate = params.strobeRate !== undefined ? params.strobeRate : 0.5;
        var t          = inputs._time      !== undefined ? inputs._time      : 0;

        return fixtures.map(function(f, i) {
            var phase = (t * rate + i * spread / Math.max(n, 1)) % 1;
            return { shutterOpen: (phase < duty) ? 1 : 0, strobeRate: strobeRate };
        });
    };

    return effect;
})();
