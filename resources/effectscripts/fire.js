/*
  QLC+ Effect Script: Fire
  Warm, flickering fire simulation on RGB fixtures.
  Each fixture independently flickers through orange/red/yellow.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Fire";
    effect.description = "Warm flickering fire colors — independent per fixture";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb"];
    effect.notes = "Produces an organic warm fire flicker using a mix of incommensurable sine waves. Each fixture flickers independently. Coolness blends from orange-red towards white. No state is needed — the algorithm is fully deterministic from elapsed time.";

    effect.inputs = [];

    effect.palettes = [];

    effect.parameters = [
        { name: "speed",     description: "Flicker speed multiplier",    min: 0.1, max: 5.0, defaultValue: 1.0  },
        { name: "intensity", description: "Overall brightness (0-1)",    min: 0.0, max: 1.0, defaultValue: 1.0  },
        { name: "coolness",  description: "Add blue (0=fire, 1=ice)",    min: 0.0, max: 1.0, defaultValue: 0.0  }
    ];

    // Seeded noise using sum of incommensurable sines — no state needed
    function flicker(t, seed, speed) {
        var s = speed;
        return 0.5 + 0.25 * Math.sin(t * s * 1.0 + seed * 7.3)
                   + 0.15 * Math.sin(t * s * 2.3 + seed * 3.1)
                   + 0.10 * Math.sin(t * s * 5.7 + seed * 11.9);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t     = inputs._time !== undefined ? inputs._time : 0;
        var speed = params.speed     !== undefined ? params.speed     : 1.0;
        var inten = params.intensity !== undefined ? params.intensity : 1.0;
        var cool  = params.coolness  !== undefined ? params.coolness  : 0.0;

        return fixtures.map(function(f, i) {
            var flame  = flicker(t, i * 1.618, speed);  // 0..~1
            flame = Math.max(0, Math.min(1, flame));

            // Fire palette: dim = deep red, bright = yellow-white
            // At flame=0: (64,0,0); at flame=0.5: (255,60,0); at flame=1: (255,200,20)
            var r, g, b;
            if (flame < 0.5) {
                var f01 = flame * 2;
                r = Math.round(64  + f01 * (255 - 64));
                g = Math.round(0   + f01 * 60);
                b = 0;
            } else {
                var f01 = (flame - 0.5) * 2;
                r = 255;
                g = Math.round(60  + f01 * (200 - 60));
                b = Math.round(0   + f01 * 20);
            }

            // Apply coolness: shift toward blue-white
            r = Math.round(r * (1 - cool) + 200 * cool);
            g = Math.round(g * (1 - cool) + 220 * cool);
            b = Math.round(b * (1 - cool) + 255 * cool);

            r = Math.round(r * inten);
            g = Math.round(g * inten);
            b = Math.round(b * inten);

            var intent = { r: r, g: g, b: b };
            if (f.hasDimmer) intent.dimmer = flame * inten;
            return intent;
        });
    };

    return effect;
})()
