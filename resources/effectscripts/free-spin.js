/*
  QLC+ Effect Script: Free Spin
  Spins heads that turn round and round on pan or tilt (free, endless or
  continuous rotation) the way their own definitions say to: a channel of
  its own, or a band on the pan channel. Asks for a direction and a speed,
  never a DMX number -- the host turns "clockwise at 40%" into each
  fixture's own value (fixture.spin, panSpin/tiltSpin; see
  docs/dev/EFFECTSCRIPT_API.md). Heads that can't spin are left alone.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Free Spin";
    effect.description = "Spin heads with free (endless) pan or tilt rotation";
    effect.author      = "qlcconsole";
    // Only heads whose definition gives them free pan/tilt rotation.
    effect.fixtureTypes = ["freerotate"];
    effect.notes = "Turns heads that can rotate endlessly on pan or tilt round and round, by "
                 + "their own definition's rotation ranges -- no DMX numbers to look up. "
                 + "Axis picks pan, tilt or both (a head that only spins on one turns on that "
                 + "one). Speed runs from the slowest the head goes (1) to its fastest (100); "
                 + "RPM asks for revolutions per minute instead where the definition gives them. "
                 + "Direction: clockwise, counter-clockwise, alternate fixtures, or bounce "
                 + "(reverse every so many seconds, easing through the stop). Stagger starts each "
                 + "fixture a little after the one before. Fade In brings the spin up to speed; "
                 + "the look's Fade Out takes it down. When it stops, Hold leaves a head where "
                 + "it is and Return points it back where the look points it. A head that spins "
                 + "on its pan channel itself (128-255 spins it) can only stop by pointing, so "
                 + "it always returns. Heads with no free rotation are left to the look.";

    effect.parameters = [
        { name: "axis",      description: "Axis", values: ["Pan", "Tilt", "Pan and tilt"], defaultValue: 0 },
        { name: "speed",     description: "Speed (1 = slowest the head goes, 100 = fastest)", min: 1, max: 100, defaultValue: 30 },
        { name: "rpm",       description: "RPM, where the definition gives it (0 = off: use Speed)", min: 0, max: 200, defaultValue: 0 },
        { name: "direction", description: "Direction", values: ["Clockwise", "Counter-clockwise", "Alternate fixtures", "Bounce"], defaultValue: 0 },
        { name: "bounce",    description: "Seconds between bounces", min: 1, max: 120, defaultValue: 8 },
        { name: "stagger",   description: "Stagger seconds per fixture (each starts this long after the one before)", min: 0, max: 10, defaultValue: 0 },
        { name: "fadeIn",    description: "Fade-in seconds (to come up to speed)", min: 0, max: 30, defaultValue: 2 },
        { name: "stop",      description: "When it stops", values: ["Hold where it stopped", "Return to the look's position"], defaultValue: 1 }
    ];

    function num(v, d) { return v !== undefined && v !== null && !isNaN(v) ? Number(v) : d; }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t       = num(inputs._time, 0);
        var fade    = num(inputs._fade, 1);
        var axis    = Math.round(num(params.axis, 0));
        var speed   = Math.max(0, Math.min(100, num(params.speed, 30))) / 100;
        var rpm     = num(params.rpm, 0);
        var dirMode = Math.round(num(params.direction, 0));
        var bounce  = Math.max(0.5, num(params.bounce, 8));
        var stagger = Math.max(0, num(params.stagger, 0));
        var fadeIn  = Math.max(0, num(params.fadeIn, 2));
        var ret     = Math.round(num(params.stop, 1)) === 1;

        // Which fixture this is, counting fixtures not heads: both arms of a
        // two-arm beam start, and go round, together.
        var order = {}, n = 0;
        fixtures.forEach(function(f) { if (order[f.id] === undefined) order[f.id] = n++; });

        return fixtures.map(function(f) {
            var out = {};
            if (!f.spin)
                return out;                         // no free rotation: the look's
            var idx = order[f.id];
            var tt = t - idx * stagger;             // this fixture's own time

            // How far up to speed: its fade in, and the look's fade in/out.
            var ramp = tt <= 0 ? 0 : (fadeIn > 0 ? Math.min(1, tt / fadeIn) : 1);
            ramp *= Math.max(0, Math.min(1, fade));
            // Turning at all: a bounce passes through its reversals at the
            // slowest it goes, it doesn't stop there.
            var on = ramp > 0.0005;

            // Which way round.
            var sign = 1;
            if (dirMode === 1) sign = -1;
            else if (dirMode === 2) sign = (idx % 2) ? -1 : 1;
            else if (dirMode === 3 && tt > 0) {
                var p = tt / bounce;
                sign = (Math.floor(p) % 2) ? -1 : 1;
                // Ease through the stop: down into each reversal and up out
                // of it, a quarter of the time each way.
                var frac = p - Math.floor(p);
                var edge = (1 - frac) * bounce;                     // to the next reversal
                if (Math.floor(p) >= 1)
                    edge = Math.min(edge, frac * bounce);           // from the last one
                ramp *= Math.min(1, edge / (bounce / 4));
            }

            function axisOut(name, a, range, aim) {
                if (!a) return;
                var s = speed;
                if (rpm > 0 && a.rpm)
                    s = Math.max(0, Math.min(1, (rpm - a.rpm[0]) / Math.max(1e-6, a.rpm[1] - a.rpm[0])));
                var v = s * ramp;
                if (on) {
                    out[name + "Spin"] = sign * Math.max(v, 0.001);
                    return;
                }
                // Stopped: its stop value, and back where the look points
                // it when asked to (a band can only stop by pointing).
                out[name + "Spin"] = 0;
                if ((ret || a.band) && aim !== undefined)
                    out[name] = aim * range;
            }

            var la = f.lookAim;
            if (axis === 0 || axis === 2)
                axisOut("pan", f.spin.pan, f.panRange, la ? la.pan : undefined);
            if (axis === 1 || axis === 2)
                axisOut("tilt", f.spin.tilt, f.tiltRange, la ? la.tilt : undefined);
            // A head that spins on one axis only turns on that one.
            if (axis === 0 && !f.spin.pan)
                axisOut("tilt", f.spin.tilt, f.tiltRange, la ? la.tilt : undefined);
            if (axis === 1 && !f.spin.tilt)
                axisOut("pan", f.spin.pan, f.panRange, la ? la.pan : undefined);
            return out;
        });
    };

    return effect;
})()
