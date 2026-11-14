/*
  QLC+ Effect Script: Fire
  Warm, flickering fire simulation on RGB fixtures.
  Each fixture independently flickers through orange/red/yellow.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Firelight";
    effect.description = "Warm flickering fire colors — independent per fixture";
    effect.author      = "QLC+";
    effect.requires    = ["fire"];
    effect.fixtureTypes = ["rgb"];
    effect.notes = "Produces an organic warm fire flicker using a mix of incommensurable sine waves. Each fixture flickers independently. Coolness blends from orange-red towards white. No state is needed — the algorithm is fully deterministic from elapsed time.";

    effect.inputs = [];

    effect.palettes = [];

    effect.parameters = [
        { name: "speed",     description: "Flicker speed multiplier",    min: 0.1, max: 5.0, defaultValue: 1.0  },
        { name: "intensity", description: "Overall brightness (0-1)",    min: 0.0, max: 1.0, defaultValue: 1.0  },
        { name: "coolness",  description: "Add blue (0=fire, 1=ice)",    min: 0.0, max: 1.0, defaultValue: 0.0  }
    ];

    // The flame and its colours are the shared fire (lib/fire.js), which
    // Fire Spread burns with too.
    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t     = inputs._time !== undefined ? inputs._time : 0;
        var speed = params.speed     !== undefined ? params.speed     : 1.0;
        var inten = params.intensity !== undefined ? params.intensity : 1.0;
        var cool  = params.coolness  !== undefined ? params.coolness  : 0.0;

        return fixtures.map(function(f, i) {
            var flame  = FIRE.flame(t, i * 1.618, speed);  // 0..1
            var intent = FIRE.colour(flame, cool, inten);
            if (f.hasDimmer) intent.dimmer = flame * inten;
            return intent;
        });
    };

    return effect;
})()
