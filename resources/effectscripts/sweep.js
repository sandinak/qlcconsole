/*
  QLC+ Effect Script: Sweep
  All moving heads scan left–right (pan) or up–down (tilt) together,
  with an optional cascade delay so they ripple rather than move in
  perfect unison. Bounce or loop mode.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Sweep";
    effect.description  = "Synchronized pan or tilt scanner sweep with cascade";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Sweeps all fixtures back and forth (bounce) or in a looping scan. Cascade staggers each fixture in time so they look like a rolling wave rather than a synchronized block. panSweep and tiltSweep independently control amplitude on each axis — combine for diagonal or elliptical sweeps. Beat sync: when BPM is active, reversal happens on the beat.";

    effect.inputs = [
        { name: "center", description: "Sweep center (0=left/up, 0.5=mid, 1=right/down)", defaultValue: 0.5 }
    ];

    // Position-only effect: no colour/dimmer. Add a Colour/Dimmer look to the
    // scene for those; this effect drives pan/tilt and composes with them.

    effect.parameters = [
        { name: "speed",     description: "Sweeps per second",                              min: 0.05, max: 4.0,  defaultValue: 0.4  },
        { name: "panSweep",  description: "Pan half-width (fraction of range)",             min: 0.0,  max: 0.5,  defaultValue: 0.35 },
        { name: "tiltSweep", description: "Tilt half-width (fraction of range)",            min: 0.0,  max: 0.5,  defaultValue: 0.0  },
        { name: "cascade",   description: "Time stagger between fixtures (fraction of period)", min: 0.0, max: 0.5, defaultValue: 0.1 },
        { name: "mode",      description: "Mode", defaultValue: 0,
          values: ["Bounce", "Loop"] },
        { name: "beatSync",  description: "Reverse/reset on beat (0=off, 1=on)",            min: 0.0,  max: 1.0,  defaultValue: 0.0  }
    ];

    // Triangle wave: 0→1→0 over one period (bounce), or 0→1 sawtooth (loop)
    function triangle(phase) {
        var p = phase - Math.floor(phase); // 0..1
        return p < 0.5 ? p * 2 : (1 - p) * 2;
    }
    function sawtooth(phase) {
        return phase - Math.floor(phase);
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t         = inputs._time  !== undefined ? inputs._time  : 0;
        var beat      = inputs._beat  !== undefined ? inputs._beat  : 0;
        var bpm       = inputs._bpm   !== undefined ? inputs._bpm   : 0;
        var beatCount = inputs._beatCount !== undefined ? inputs._beatCount : 0;
        var center    = inputs.center !== undefined ? inputs.center : 0.5;

        var speed     = params.speed     !== undefined ? params.speed     : 0.4;
        var panSweep  = params.panSweep  !== undefined ? params.panSweep  : 0.35;
        var tiltSweep = params.tiltSweep !== undefined ? params.tiltSweep : 0.0;
        var cascade   = params.cascade   !== undefined ? params.cascade   : 0.1;
        var mode      = params.mode      !== undefined ? Math.round(params.mode) : 0;
        var beatSync  = params.beatSync  !== undefined ? params.beatSync  : 0.0;

        var n = Math.max(fixtures.length, 1);

        // Base phase from time or beat
        var basePhase;
        if (beatSync > 0.5 && bpm > 0)
            basePhase = (beatCount + beat) * 0.5; // one sweep per 2 beats
        else
            basePhase = t * speed;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            // Cascade: each fixture is staggered forward in time
            var stagger = (n > 1) ? (i / (n - 1)) * cascade : 0;
            var phase = basePhase - stagger;

            var wave = (mode === 1) ? sawtooth(phase) : triangle(phase);

            // wave is 0..1; map to center ± sweep
            var pan  = (center - panSweep  + wave * panSweep  * 2) * f.panRange;
            var tilt = (center - tiltSweep + wave * tiltSweep * 2) * f.tiltRange;

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
