/*
  QLC+ Effect Script: Gobo Chase
  Steps through gobo wheel positions across the fixture array.
  Each fixture receives a different gobo index, creating a travelling
  gobo pattern. Emits the `gobo` intent (0-based index into the wheel).
*/
(function() {
    var effect = new Object;
    effect.name        = "Gobo Chase";
    effect.description = "Steps through gobo wheel positions across the fixture array";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.fixtureTypes = ["moving", "spot", "beam"];

    effect.parameters = [
        { name: "rate",   description: "Rate (gobos/s)", type: "float", min: 0.1, max: 10, defaultValue: 1 },
        { name: "spread", description: "Spread (steps)", type: "float", min: 1,   max: 12, defaultValue: 4 },
        { name: "count",  description: "Gobo count",     type: "float", min: 2,   max: 12, defaultValue: 6 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n      = fixtures.length;
        var rate   = params.rate   !== undefined ? params.rate   : 1;
        var spread = params.spread !== undefined ? params.spread : 4;
        var count  = Math.max(2, Math.round(params.count !== undefined ? params.count : 6));
        var t      = inputs._time  !== undefined ? inputs._time  : 0;

        return fixtures.map(function(f, i) {
            var idx = Math.floor(t * rate + i * spread / Math.max(n, 1)) % count;
            return { gobo: (idx + count) % count };
        });
    };

    return effect;
})();
