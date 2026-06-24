/*
  QLC+ Effect Script: Spotlight Chase
  A single "hot" fixture is fully lit while neighboring fixtures trail
  off with a dimmer tail. The hot spot travels across the array.
  Great for shooting-star, highlight-sweep, and follow-the-leader looks.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Spotlight Chase";
    effect.description  = "Hot spot chases across the rig with a dimmer trail";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving", "dimmer", "rgb"];
    effect.notes = "A bright 'hot spot' travels along the fixture array while trailing fixtures dim out. Position controls where the hot spot is (0=first fixture, 1=last), so you can drive it with a joystick or input. Auto mode ignores position and moves the spot automatically at speed cycles/sec. Trail controls how far the dimmer falloff reaches; trailShape 0=linear, 1=cosine. All heads aim at the same position as the hot spot (pan/tilt follows). Beat sync: the spot snaps to the next fixture on each beat.";

    effect.inputs = [
        { name: "position", description: "Hot-spot position override (0=first, 1=last; auto when no BPM)", defaultValue: 0.0 }
    ];

    effect.palettes = [
        { name: "color",  type: "Color",  optional: true },
        { name: "dimmer", type: "Dimmer", optional: true },
        { name: "pos",    type: "PanTilt", optional: true }
    ];

    effect.parameters = [
        { name: "speed",      description: "Hot-spot cycles per second (auto mode)",         min: 0.1,  max: 5.0,  defaultValue: 1.0  },
        { name: "trail",      description: "Trail length (fixture-widths)",                  min: 0.0,  max: 3.0,  defaultValue: 1.0  },
        { name: "trailShape", description: "Trail shape", min: 0, max: 1, defaultValue: 1,
          values: ["Linear", "Cosine"] },
        { name: "mode",       description: "Direction", min: 0, max: 2, defaultValue: 0,
          values: ["Loop", "Bounce", "Input"] },
        { name: "beatSync",   description: "Advance one fixture per beat (0=off, 1=on)",     min: 0.0,  max: 1.0,  defaultValue: 0.0  },
        { name: "panCenter",  description: "Pan center for all heads (0=left, 0.5=mid)",     min: 0.0,  max: 1.0,  defaultValue: 0.5  },
        { name: "tiltCenter", description: "Tilt center for all heads (0=up, 0.5=mid)",      min: 0.0,  max: 1.0,  defaultValue: 0.5  }
    ];

    function dimForDistance(dist, trail, shape) {
        if (trail <= 0) return (dist < 0.5) ? 1.0 : 0.0;
        if (dist > trail) return 0.0;
        var t = 1.0 - dist / trail;          // 1 at center, 0 at edge
        if (shape >= 0.5)
            return (Math.cos((1 - t) * Math.PI) + 1) / 2;  // cosine rolloff
        return t;                                            // linear
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t         = inputs._time      !== undefined ? inputs._time      : 0;
        var beat      = inputs._beat      !== undefined ? inputs._beat      : 0;
        var bpm       = inputs._bpm       !== undefined ? inputs._bpm       : 0;
        var beatCount = inputs._beatCount !== undefined ? inputs._beatCount : 0;
        var posInput  = inputs.position   !== undefined ? inputs.position   : 0.0;

        var speed      = params.speed      !== undefined ? params.speed      : 1.0;
        var trail      = params.trail      !== undefined ? params.trail      : 1.0;
        var trailShape = params.trailShape !== undefined ? params.trailShape : 1.0;
        var mode       = params.mode       !== undefined ? Math.round(params.mode) : 0;
        var beatSync   = params.beatSync   !== undefined ? params.beatSync   : 0.0;
        var panCenter  = params.panCenter  !== undefined ? params.panCenter  : 0.5;
        var tiltCenter = params.tiltCenter !== undefined ? params.tiltCenter : 0.5;

        var n = Math.max(fixtures.length, 1);

        // Compute hot-spot fractional index (0..n-1)
        var hotPos; // in fixture-units
        if (mode === 2) {
            // Input mode: position input directly drives the hot spot
            hotPos = posInput * (n - 1);
        } else if (beatSync > 0.5 && bpm > 0) {
            // Beat mode: advance one fixture per beat
            var raw = beatCount % (mode === 1 ? (2 * n - 2) : n);
            if (mode === 1 && raw >= n)
                raw = (2 * n - 2) - raw;  // bounce back
            hotPos = raw + beat;           // smooth interpolation within beat
        } else {
            var phase = t * speed;
            if (mode === 1) {
                // Bounce: triangle wave 0..1..0 per cycle
                var p = phase - Math.floor(phase);
                var wave = p < 0.5 ? p * 2 : (1 - p) * 2;
                hotPos = wave * (n - 1);
            } else {
                // Loop: sawtooth
                hotPos = (phase - Math.floor(phase)) * n;
            }
        }

        // Palette position override
        var aimPan  = panCenter;
        var aimTilt = tiltCenter;
        if (palettes.pos && palettes.pos.pan !== undefined) {
            aimPan  = palettes.pos.pan  / 360.0;
            aimTilt = palettes.pos.tilt / 270.0;
        }

        // Peak dimmer from palette or default full
        var peakDim = 1.0;
        if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
            peakDim = palettes.dimmer.dimmer;

        return fixtures.map(function(f, i) {
            var dist = Math.abs(i - hotPos);
            var bright = dimForDistance(dist, trail, trailShape) * peakDim;

            var intent = {};

            if (f.hasDimmer)
                intent.dimmer = bright;

            if (f.hasPanTilt) {
                intent.pan  = aimPan  * f.panRange;
                intent.tilt = aimTilt * f.tiltRange;
            }

            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = Math.round(palettes.color.r * bright);
                intent.g = Math.round(palettes.color.g * bright);
                intent.b = Math.round(palettes.color.b * bright);
            } else {
                intent.r = Math.round(255 * bright);
                intent.g = Math.round(255 * bright);
                intent.b = Math.round(255 * bright);
            }

            return intent;
        });
    };

    return effect;
})()
