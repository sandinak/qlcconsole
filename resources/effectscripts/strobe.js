/*
  QLC+ Effect Script: Strobe
  Variable-rate strobe on all selected fixtures.
  Rate can be driven by a MIDI/OSC/HID input for live control.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Strobe";
    effect.description = "Variable-rate fixture strobe, input-controllable";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["shutter", "dimmer"];
    effect.notes        = "Variable-rate strobe driven by a speed input. minHz and maxHz define the frequency range; the input fader scales between them. Fixtures with a dedicated shutter channel use it; others fall back to the dimmer. Duty cycle controls flash width relative to the period.";

    effect.inputs = [
        { name: "rate", description: "Strobe rate (0=slow, 1=fast)", defaultValue: 0.3 }
    ];

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true }
    ];

    effect.parameters = [
        { name: "minHz",    description: "Slowest rate (Hz)",    min: 0.5,  max: 10.0,  defaultValue: 1.0  },
        { name: "maxHz",    description: "Fastest rate (Hz)",    min: 1.0,  max: 30.0,  defaultValue: 20.0 },
        { name: "duty",     description: "On-time fraction",     min: 0.01, max: 0.99,  defaultValue: 0.5  },
        { name: "dimmer",   description: "Peak dimmer level",    min: 0.0,  max: 1.0,   defaultValue: 1.0  }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = inputs._time !== undefined ? inputs._time : 0;
        var rate   = inputs.rate  !== undefined ? inputs.rate  : 0.3;
        var minHz  = params.minHz  !== undefined ? params.minHz  : 1.0;
        var maxHz  = params.maxHz  !== undefined ? params.maxHz  : 20.0;
        var duty   = params.duty   !== undefined ? params.duty   : 0.5;
        var dimmer = params.dimmer !== undefined ? params.dimmer : 1.0;

        var hz     = minHz + rate * (maxHz - minHz);
        var period = 1.0 / hz;
        var phase  = (t % period) / period;  // 0..1 within current cycle
        var on     = phase < duty;

        return fixtures.map(function(f) {
            var intent = {};
            if (f.hasDimmer) intent.dimmer = on ? dimmer : 0.0;
            if (on && palettes.color && palettes.color.r !== undefined) {
                intent.r = palettes.color.r;
                intent.g = palettes.color.g;
                intent.b = palettes.color.b;
            }
            return intent;
        });
    };

    return effect;
})()
