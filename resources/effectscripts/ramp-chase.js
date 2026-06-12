/*
  QLC+ Effect Script: Ramp Chase
  A dimmer ramp (or color wash) travels through the fixture array.
  The head of the ramp is bright; fixtures behind it trail off.
  Classic "chaser" look that works well on linear rig positions.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Ramp Chase";
    effect.description = "Dimmer ramp chases through the fixture array";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["dimmer", "rgb"];
    effect.notes = "A bright ramp (like a comet tail) travels through the fixture array. The head position is a fraction 0-1 of the array length; rampLen sets how much of the array trails behind it. Bind the speed input to a fader for tap-to-tempo or live speed control. Bounce for ping-pong; reverse for backwards.";

    effect.inputs = [
        { name: "speed", description: "Chase speed (0=stop, 1=fast)", defaultValue: 0.5 }
    ];

    effect.palettes = [
        { name: "color",      type: "Color",  optional: true },
        { name: "background", type: "Color",  optional: true }
    ];

    effect.parameters = [
        { name: "maxSpeed",  description: "Max fixtures/sec",          min: 0.1, max: 30.0, defaultValue: 5.0  },
        { name: "rampLen",   description: "Ramp tail length (0-1 of array)", min: 0.0, max: 1.0, defaultValue: 0.4 },
        { name: "peakDim",   description: "Peak dimmer at head",       min: 0.0, max: 1.0,  defaultValue: 1.0  },
        { name: "reverse",   description: "Reverse direction",         min: 0.0, max: 1.0,  defaultValue: 0.0  },
        { name: "bounce",    description: "Bounce (ping-pong)",        min: 0.0, max: 1.0,  defaultValue: 0.0  }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t        = inputs._time !== undefined ? inputs._time : 0;
        var speedN   = inputs.speed !== undefined ? inputs.speed : 0.5;

        var maxSpeed = params.maxSpeed !== undefined ? params.maxSpeed : 5.0;
        var rampLen  = params.rampLen  !== undefined ? params.rampLen  : 0.4;
        var peakDim  = params.peakDim  !== undefined ? params.peakDim  : 1.0;
        var rev      = params.reverse  !== undefined ? params.reverse  : 0.0;
        var bounce   = params.bounce   !== undefined ? params.bounce   : 0.0;

        var n = fixtures.length;
        if (n === 0) return [];

        var speed = speedN * maxSpeed;  // fixtures per second
        var raw   = (t * speed / n) % 1.0;  // head position 0..1

        // Bounce: triangle wave instead of sawtooth
        var head = raw;
        if (bounce > 0.5) {
            head = (raw < 0.5) ? raw * 2 : (1.0 - raw) * 2;
        }

        if (rev > 0.5) head = 1.0 - head;

        var fc = (palettes.color && palettes.color.r !== undefined)
                 ? palettes.color : { r: 255, g: 255, b: 255 };
        var bg = (palettes.background && palettes.background.r !== undefined)
                 ? palettes.background : null;

        return fixtures.map(function(f, i) {
            var pos = i / n;
            // Distance behind the head (toroidal)
            var behind = head - pos;
            if (behind < 0) behind += 1.0;

            // Ramp: full at head (behind=0), zero at rampLen
            var bright = (behind < rampLen && rampLen > 0)
                         ? (1.0 - behind / rampLen) : 0.0;
            bright = bright * peakDim;

            var intent = {};
            if (f.hasDimmer) intent.dimmer = bg ? peakDim : bright;

            if (bright > 0) {
                intent.r = Math.round(fc.r * bright / peakDim);
                intent.g = Math.round(fc.g * bright / peakDim);
                intent.b = Math.round(fc.b * bright / peakDim);
            } else if (bg) {
                intent.r = bg.r; intent.g = bg.g; intent.b = bg.b;
            } else {
                intent.r = 0; intent.g = 0; intent.b = 0;
            }
            return intent;
        });
    };

    return effect;
})()
