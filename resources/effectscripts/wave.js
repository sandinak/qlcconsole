/*
  QLC+ Effect Script: Wave
  A sinusoidal traveling wave propagates along the fixture array in pan
  and/or tilt. Classic "stadium wave" for overhead moving-head rigs.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Wave";
    effect.description  = "Traveling sine wave across the fixture array";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "A sinusoidal wave travels along the fixture array. Pan and tilt amplitudes are set independently so you can do a pure pan wave, a pure tilt wave, or an elliptical wave. Speed controls wave cycles per second; wavelength controls how many fixture-spacings fit one full cycle (lower = tighter wave). Beat sync: when the host has an active BPM source, beatSync snaps the wave so one full traversal = one beat.";

    effect.inputs = [];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "speed",      description: "Cycles per second (+ = left→right, − = reverse)", min: -5.0, max: 5.0,  defaultValue: 0.5  },
        { name: "panAmp",     description: "Pan amplitude (fraction of total range)",          min: 0.0,  max: 0.5,  defaultValue: 0.2  },
        { name: "tiltAmp",    description: "Tilt amplitude (fraction of total range)",         min: 0.0,  max: 0.5,  defaultValue: 0.0  },
        { name: "panCenter",  description: "Pan center (0=left, 0.5=mid, 1=right)",            min: 0.0,  max: 1.0,  defaultValue: 0.5  },
        { name: "tiltCenter", description: "Tilt center (0=up, 0.5=mid, 1=down)",              min: 0.0,  max: 1.0,  defaultValue: 0.5  },
        { name: "wavelength", description: "Fixtures per full wave cycle (1=one wave spans rig)", min: 0.25, max: 4.0, defaultValue: 1.0 },
        { name: "beatSync",   description: "Lock speed to BPM (0=off, 1=1 wave/beat, 2=2/beat)", min: 0.0, max: 4.0, defaultValue: 0.0  }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t          = inputs._time  !== undefined ? inputs._time  : 0;
        var beat       = inputs._beat  !== undefined ? inputs._beat  : 0;
        var bpm        = inputs._bpm   !== undefined ? inputs._bpm   : 0;
        var beatCount  = inputs._beatCount !== undefined ? inputs._beatCount : 0;

        var speed      = params.speed      !== undefined ? params.speed      : 0.5;
        var panAmp     = params.panAmp     !== undefined ? params.panAmp     : 0.2;
        var tiltAmp    = params.tiltAmp    !== undefined ? params.tiltAmp    : 0.0;
        var panCenter  = params.panCenter  !== undefined ? params.panCenter  : 0.5;
        var tiltCenter = params.tiltCenter !== undefined ? params.tiltCenter : 0.5;
        var wavelength = params.wavelength !== undefined ? params.wavelength : 1.0;
        var beatSync   = params.beatSync   !== undefined ? params.beatSync   : 0.0;

        var n = Math.max(fixtures.length, 1);

        // When beatSync > 0 and a BPM source is active, derive time from
        // beat phase so the wave locks to the music. beatSync sets how many
        // wave cycles complete per beat.
        var waveT;
        if (beatSync > 0 && bpm > 0)
            waveT = (beatCount + beat) * beatSync;
        else
            waveT = t * speed;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            // Phase for this fixture: spatial offset proportional to index,
            // scaled so `wavelength` fixtures span one full cycle.
            var spatialPhase = (n > 1) ? (i / (n - 1)) / wavelength : 0;
            var phase = (waveT - spatialPhase) * 2 * Math.PI;

            var pan  = (panCenter  + panAmp  * Math.sin(phase)) * f.panRange;
            var tilt = (tiltCenter + tiltAmp * Math.cos(phase)) * f.tiltRange;

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
