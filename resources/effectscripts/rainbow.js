/*
  QLC+ Effect Script: Rainbow
  Cycles fixtures through the full colour spectrum, spreading the hue
  across the fixture array so adjacent lights show different colours.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Rainbow";
    effect.description = "Continuous hue rotation across all selected fixtures";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb"];
    effect.notes = "Continuously rotates hue across the full spectrum. Speed controls rotation rate; spread staggers starting hues so adjacent fixtures show different colours simultaneously. Works with any fixture that has additive RGB (or CMY) colour mixing.";

    effect.inputs = [];  // time-driven only

    effect.palettes = [];  // no palette slots — script generates all colours

    effect.parameters = [
        { name: "speed",  description: "Full-cycle seconds",                 min: 0.5,  max: 60.0, defaultValue: 5.0  },
        { name: "spread", description: "Hue spread across fixtures (0-1=full circle)", min: 0.0, max: 1.0, defaultValue: 1.0 },
        { name: "sat",    description: "Saturation (0=white, 1=full colour)", min: 0.0,  max: 1.0,  defaultValue: 1.0  },
        { name: "dimmer", description: "Dimmer level (0-1)",                 min: 0.0,  max: 1.0,  defaultValue: 1.0  }
    ];

    // HSV → RGB conversion (s, v in 0..1, h in 0..360)
    function hsvToRgb(h, s, v) {
        h = ((h % 360) + 360) % 360;
        var c  = v * s;
        var x  = c * (1 - Math.abs((h / 60) % 2 - 1));
        var m  = v - c;
        var r1, g1, b1;
        if      (h < 60)  { r1 = c; g1 = x; b1 = 0; }
        else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
        else              { r1 = c; g1 = 0; b1 = x; }
        return {
            r: Math.round((r1 + m) * 255),
            g: Math.round((g1 + m) * 255),
            b: Math.round((b1 + m) * 255)
        };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = inputs._time !== undefined ? inputs._time : 0;
        var speed  = params.speed  !== undefined ? params.speed  : 5.0;
        var spread = params.spread !== undefined ? params.spread : 1.0;
        var sat    = params.sat    !== undefined ? params.sat    : 1.0;
        var dim    = params.dimmer !== undefined ? params.dimmer : 1.0;

        var baseHue = (t / speed * 360) % 360;
        var n = Math.max(fixtures.length, 1);

        return fixtures.map(function(f, i) {
            var hue  = baseHue + (i / n) * spread * 360;
            var rgb  = hsvToRgb(hue, sat, dim);
            var intent = { r: rgb.r, g: rgb.g, b: rgb.b };
            if (f.hasDimmer) intent.dimmer = dim;
            return intent;
        });
    };

    return effect;
})()
