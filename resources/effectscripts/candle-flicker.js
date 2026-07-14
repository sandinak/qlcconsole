/*
  QLC+ Effect Script: Candle Flicker
  Intensity-ONLY organic brightness flicker. Emits `dimmer` as a 0..1
  *multiplier* on each fixture's scene-assigned brightness — so the host scales
  the level a Dimmer look sets (and the colour a Colour look sets) rather than
  overriding it. Works on any fixture: master-dimmer fixtures scale the master
  channel, colour-only fixtures scale their RGB.

  Design intent (per user):
    - Colour comes from a Colour look, base level from a Dimmer look.
    - This script only supplies the *variance* around that base.
    - Each fixture flickers INDEPENDENTLY (decorrelated), never in unison.

  Uses three incommensurable sine waves with a per-fixture seed so every head
  gets its own non-repeating, natural flame.
*/
(function() {
    var effect = new Object;
    effect.name        = "Candle Flicker";
    effect.description = "Organic per-fixture brightness flicker (intensity only)";
    effect.apiVersion  = 1;
    effect.version     = 2;
    // Any fixture that carries a brightness — master dimmer OR colour emitters.
    effect.fixtureTypes = [
        "rgb", "rgbw", "rgba", "rgbwa", "rgbwauv",
        "wash", "spot", "beam", "dimmer", "color",
        "moving", "mover", "movers"
    ];

    effect.parameters = [
        { name: "rate",  description: "Flicker rate (Hz)",           type: "float", min: 0.1, max: 8, defaultValue: 1.4 },
        { name: "depth", description: "Flicker depth (0=steady,1=guttering)", type: "float", min: 0, max: 1, defaultValue: 0.55 },
        { name: "level", description: "Peak level (× the look's base)", type: "float", min: 0, max: 1, defaultValue: 1.0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var rate  = params.rate  !== undefined ? params.rate  : 1.4;
        var depth = params.depth !== undefined ? params.depth : 0.55;
        var level = params.level !== undefined ? params.level : 1.0;
        var t     = inputs._time !== undefined ? inputs._time : 0;

        return fixtures.map(function(f, i) {
            // Per-fixture seed decorrelates each flame (irrational offsets so no
            // two heads share a phase and the pattern never repeats).
            var s = i * 0.6180339887;   // golden-ratio spacing
            var p = 2 * Math.PI * (t * rate);
            var flame = 0.5
                + 0.32 * Math.sin(p          + s * 6.283)
                + 0.13 * Math.sin(p * 2.371  + s * 11.17)
                + 0.09 * Math.sin(p * 5.813  + s * 3.57);
            flame = Math.max(0, Math.min(1, flame));   // 0..1, ~centred on 0.5

            // Map to a multiplier that peaks at `level` and guttes down by `depth`.
            var mul = level - depth * (1 - flame);
            return { dimmer: Math.max(0, Math.min(1, mul)) };
        });
    };

    return effect;
})();
