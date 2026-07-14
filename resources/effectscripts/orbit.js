/*
  QLC+ Effect Script: Orbit
  All fixtures orbit around a joystick-positioned center point.
  Each fixture gets an equal phase offset around the circle.
  Great for ring/wash rigs where you want movement as a unit.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Orbit";
    effect.description = "Fixtures orbit a joystick-controlled center in unison";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "All fixtures orbit a joystick-controlled centre point in a circle. Phase spread staggers fixtures around the orbit so they form a rotating ring rather than moving in unison. Useful for dramatic aerial swings or follow-the-leader chase patterns on a tight moving-head array.";

    effect.inputs = [
        { name: "x", description: "Orbit center pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Orbit center tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "speed",    description: "Orbits per second",             min: -5.0, max: 5.0,  defaultValue: 0.5  },
        { name: "panRadius",  description: "Pan radius (fraction of range)",  min: 0.0,  max: 0.5,  defaultValue: 0.15 },
        { name: "tiltRadius", description: "Tilt radius (fraction of range)", min: 0.0,  max: 0.5,  defaultValue: 0.10 },
        { name: "phaseSpread", description: "Phase spread (0=clump, 1=full circle)", min: 0.0, max: 1.0, defaultValue: 1.0 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t    = inputs._time !== undefined ? inputs._time : 0;
        var cx   = inputs.x !== undefined ? inputs.x : 0.5;
        var cy   = inputs.y !== undefined ? inputs.y : 0.5;

        var speed      = params.speed      !== undefined ? params.speed      : 0.5;
        var panR       = params.panRadius  !== undefined ? params.panRadius  : 0.15;
        var tiltR      = params.tiltRadius !== undefined ? params.tiltRadius : 0.10;
        var spread     = params.phaseSpread !== undefined ? params.phaseSpread : 1.0;

        var n = Math.max(fixtures.length, 1);

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            var phase = t * speed * 2 * Math.PI
                        + i * spread * 2 * Math.PI / n;

            var pan  = (cx + panR  * Math.cos(phase)) * f.panRange;
            var tilt = (cy + tiltR * Math.sin(phase)) * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            // Position effect: only touch dimmer/colour when a palette is
            // bound. Otherwise omit those keys so the scene's own Dimmer/Color
            // looks fall through (Override only wins channels we emit).
            if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                intent.dimmer = palettes.dimmer.dimmer;

            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = palettes.color.r;
                intent.g = palettes.color.g;
                intent.b = palettes.color.b;
            }

            return intent;
        });
    };

    return effect;
})()
