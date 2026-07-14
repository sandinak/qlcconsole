/*
  QLC+ Effect Script: Pan/Tilt Phaser
  The core MA3 "PanTilt FX" primitive: an independent sine on pan and on tilt,
  each with its own size and rate, plus a phase offset between them and a
  per-fixture phase spread across the group.
    - equal rates + 90 deg pan/tilt phase  -> a circle (like Orbit)
    - tiltSize 0                            -> a pure pan wave (like Sweep)
    - panSize 0                             -> a pure tilt wave (like Wave)
  One configurable building block for all of those. Position-only.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Pan/Tilt Phaser";
    effect.description = "Independent pan & tilt sines with phase spread";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "A flexible movement engine. Pan and tilt each oscillate as a sine "
                 + "around a joystick-controlled centre, with separate size and rate. "
                 + "ptPhase sets the phase between the two axes (90 = circle, 0 = a "
                 + "diagonal line, 180 = the other diagonal). spread staggers fixtures "
                 + "along the wave; reverse flips the stagger direction. Set one size to "
                 + "0 for a pure pan- or tilt-only wave.";

    effect.inputs = [
        { name: "x", description: "Centre pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Centre tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "panSize",  description: "Pan size (fraction of range)",   min: 0.0, max: 0.5, defaultValue: 0.15 },
        { name: "tiltSize", description: "Tilt size (fraction of range)",  min: 0.0, max: 0.5, defaultValue: 0.10 },
        { name: "panRate",  description: "Pan cycles per second",          min: -5.0, max: 5.0, defaultValue: 0.5 },
        { name: "tiltRate", description: "Tilt cycles per second",         min: -5.0, max: 5.0, defaultValue: 0.5 },
        { name: "ptPhase",  description: "Pan/tilt phase (deg: 90=circle)", min: 0.0, max: 360.0, defaultValue: 90.0 },
        { name: "spread",   description: "Per-fixture phase spread (deg)",  min: 0.0, max: 360.0, defaultValue: 0.0 },
        { name: "reverse",  description: "Reverse spread direction",        min: 0.0, max: 1.0, defaultValue: 0.0 }
    ];

    var TAU = 2 * Math.PI;

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t  = inputs._time !== undefined ? inputs._time : 0;
        var cx = inputs.x !== undefined ? inputs.x : 0.5;
        var cy = inputs.y !== undefined ? inputs.y : 0.5;

        var panSize  = params.panSize  !== undefined ? params.panSize  : 0.15;
        var tiltSize = params.tiltSize !== undefined ? params.tiltSize : 0.10;
        var panRate  = params.panRate  !== undefined ? params.panRate  : 0.5;
        var tiltRate = params.tiltRate !== undefined ? params.tiltRate : 0.5;
        var ptPhase  = (params.ptPhase !== undefined ? params.ptPhase : 90.0) * Math.PI / 180.0;
        var spread   = (params.spread  !== undefined ? params.spread  : 0.0) * Math.PI / 180.0;
        var rev      = params.reverse  !== undefined ? params.reverse  : 0.0;

        var n = Math.max(fixtures.length - 1, 1);
        var dir = (rev > 0.5) ? -1.0 : 1.0;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            var ph = dir * i * spread / n;

            var pan  = (cx + panSize  * Math.sin(TAU * panRate  * t + ph))           * f.panRange;
            var tilt = (cy + tiltSize * Math.sin(TAU * tiltRate * t + ph + ptPhase)) * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            // Position effect: only emit dimmer/colour when a palette is bound.
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
