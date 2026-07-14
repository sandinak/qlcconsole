/*
  QLC+ Effect Script: Ballyhoo
  Wide, organic "audience scan" — the classic MA3/Hog ballyhoo. Pan and tilt
  are each driven by two incommensurate sine layers (golden-ratio frequencies)
  so the motion sweeps the room without ever settling into an obvious loop.
  Per-fixture phase spread keeps heads from moving in lockstep.
  Position-only: leaves colour/dimmer to the scene unless palettes are bound.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Ballyhoo";
    effect.description = "Wide pseudo-random audience-scan pan/tilt sweeps";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Big aerial 'search' look: each head sweeps a wide area around a "
                 + "joystick-controlled centre, layering two off-ratio sine waves so the "
                 + "path never obviously repeats. panSize/tiltSize set how far it roams "
                 + "(fraction of range); chaos blends in the second, faster layer for a "
                 + "more frantic feel; spread staggers fixtures so they scatter rather "
                 + "than march together. Great over an audience or as a high-energy build.";

    effect.inputs = [
        { name: "x", description: "Scan centre pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Scan centre tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "speed",    description: "Sweep speed (cycles per second)",        min: 0.0, max: 3.0, defaultValue: 0.4 },
        { name: "panSize",  description: "Pan sweep size (fraction of range)",     min: 0.0, max: 0.5, defaultValue: 0.35 },
        { name: "tiltSize", description: "Tilt sweep size (fraction of range)",    min: 0.0, max: 0.5, defaultValue: 0.25 },
        { name: "chaos",    description: "Second-layer amount (0=smooth, 1=wild)", min: 0.0, max: 1.0, defaultValue: 0.5 },
        { name: "spread",   description: "Per-fixture phase spread (0=clump, 1=scatter)", min: 0.0, max: 1.0, defaultValue: 1.0 }
    ];

    // Golden-ratio-derived multipliers keep the two sine layers incommensurate,
    // so pan and tilt never lock into a short repeating cycle.
    var PHI = 1.6180339887;
    var TAU = 2 * Math.PI;

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t  = inputs._time !== undefined ? inputs._time : 0;
        var cx = inputs.x !== undefined ? inputs.x : 0.5;
        var cy = inputs.y !== undefined ? inputs.y : 0.5;

        var speed    = params.speed    !== undefined ? params.speed    : 0.4;
        var panSize  = params.panSize  !== undefined ? params.panSize  : 0.35;
        var tiltSize = params.tiltSize !== undefined ? params.tiltSize : 0.25;
        var chaos    = params.chaos    !== undefined ? params.chaos    : 0.5;
        var spread   = params.spread   !== undefined ? params.spread   : 1.0;

        var n = Math.max(fixtures.length, 1);
        var w = TAU * speed;
        var mix = 1.0 + chaos; // normalise so amplitude stays ~constant

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            // Each fixture gets its own phase offset so the group scatters.
            var ph = spread * i * TAU / n;

            // Two layers per axis: a slow base sweep + a faster off-ratio wobble.
            var panW  = (Math.sin(w * t + ph)
                         + chaos * Math.sin(w * PHI * t + ph * 1.7)) / mix;
            var tiltW = (Math.sin(w * 0.8 * t + ph * 1.3 + 1.1)
                         + chaos * Math.sin(w * PHI * 1.2 * t + ph)) / mix;

            var pan  = (cx + panSize  * panW)  * f.panRange;
            var tilt = (cy + tiltSize * tiltW) * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            // Position effect: only touch dimmer/colour when a palette is bound,
            // otherwise the scene's own looks fall through (Override only wins
            // channels we emit).
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
