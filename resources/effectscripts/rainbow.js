/*
  QLC+ Effect Script: Rainbow
  Cycles fixtures through the full colour spectrum, spreading the hue
  across the fixture array so adjacent lights show different colours.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Rainbow";
    // Takes the host's spread (fixture.phase, inputs._spread) in place of its own.
    effect.hostSpread = "spread";
    effect.description = "Continuous hue rotation across all selected fixtures";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb"];
    effect.notes = "Continuously rotates hue across the full spectrum. Speed controls rotation rate; spread staggers starting hues so adjacent fixtures show different colours simultaneously. With a colour in the look it sweeps that colour instead (a wave of it across the heads); with two or more, it sweeps through the look's colours. Works with any fixture that has additive RGB (or CMY) colour mixing.";

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

        // A colour in the look: the sweep is in the look's colours, not the
        // wheel ("If I set a color on the entire set I see the color sweep
        // but no rainbow"). One colour sweeps as a wave of that colour; two
        // or more, the sweep runs through them, round and back to the first.
        var look = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        function lookColour(cycle) {
            var c = ((cycle % 1) + 1) % 1;
            if (look.length === 1) {
                var lvl = (0.5 + 0.5 * Math.cos(c * 2 * Math.PI)) * dim;
                return { r: Math.round(look[0].r * lvl), g: Math.round(look[0].g * lvl), b: Math.round(look[0].b * lvl) };
            }
            var x = c * look.length, k = Math.floor(x), f = x - k;
            var a = look[k % look.length], b = look[(k + 1) % look.length];
            return { r: Math.round((a.r + (b.r - a.r) * f) * dim),
                     g: Math.round((a.g + (b.g - a.g) * f) * dim),
                     b: Math.round((a.b + (b.b - a.b) * f) * dim) };
        }

        return fixtures.map(function(f, i) {
            var offset = inputs._spread ? f.phase : (i / n) * spread;
            var rgb = look.length > 0 ? lookColour(baseHue / 360 + offset)
                                      : hsvToRgb(baseHue + offset * 360, sat, dim);
            var intent = { r: rgb.r, g: rgb.g, b: rgb.b };
            if (f.hasDimmer) intent.dimmer = dim;
            return intent;
        });
    };

    return effect;
})()
