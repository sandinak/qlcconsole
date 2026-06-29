/*
  QLC+ Effect Script: Followspot
  Move a set of fixtures using two joystick axes (pan/tilt).
  Compatible with any gamepad or MIDI controller mapped to the x/y inputs.

  Velocity-based: the stick deflects a setpoint at a rate proportional to
  deflection.  Releasing the stick holds the current position, so the beam
  stays put without continuous pressure.  Setpoint is initialized from the
  Aim target when one is bound (followTarget slot), allowing smooth handoff
  from a pre-set scene Aim palette position.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Followspot";
    effect.description = "Pan/tilt fixtures with joystick X/Y axes";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Maps joystick X/Y axes to pan and tilt so you can fly moving heads live. This effect controls POSITION ONLY — it never touches colour or dimmer, so the scene's own Color/Dimmer palettes always retain control. Sensitivity and deadzone are global joystick settings (HID device profile / I/O Manager), not configured here. Person Height biases the tilt centre so the beam lands at the subject's head. Bind the followTarget slot to a stage target to seed the initial position from a pre-set Aim palette.";

    // The host injects joystick data automatically when a HID profile is active.
    // Manual input bindings (x / y) are kept as a fallback for non-HID controllers.
    effect.dataChannels = ["joystick"];

    effect.inputs = [
        { name: "x", description: "Pan axis fallback (0=left, 0.5=center, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Tilt axis fallback (0=up, 0.5=center, 1=down)",   defaultValue: 0.5 }
    ];

    // Position-only effect: no colour or dimmer palettes. The scene's own
    // Color/Dimmer palettes are left untouched so movement composes on top of
    // whatever look is active.
    effect.palettes = [];

    // No target binding: the followspot reads its target IMPLICITLY from the
    // scene's own Aim look.  The host injects that as fixture.sceneTarget and,
    // when present, moves the shared aim point so the heads converge — this
    // effect then defers (emits no pan/tilt).  Free-fly (no Aim look) is the
    // only case this script flies angles itself.

    // Sensitivity and deadzone are deliberately NOT effect parameters — they are
    // global joystick settings (HID device profile / I/O Manager) and arrive via
    // the joystick data channel. Only the followspot-specific person-height bias
    // and the scene-transition seeding mode are configured here.
    //
    // followMode controls where the beam starts when this effect takes over from
    // a previous scene:
    //   "lastPosition" (default) — resume from the last spot position the beam
    //       was left at, so the heads DON'T move on transitions between scenes.
    //       Falls back to the bound followTarget (then physical centre) the very
    //       first time, when no last position has been recorded yet.
    //   "snapToTarget" — snap to the bound followTarget's pre-aimed position on
    //       every transition (ignores the last spot position).
    effect.parameters = [
        { name: "followMode",   description: "On scene change: hold the last beam position, or snap to the bound followTarget", values: ["lastPosition", "snapToTarget"], defaultValue: 0 },
        { name: "personHeight", description: "Height of person being followed (cm) — adjusts tilt centre", min: 50, max: 250, defaultValue: 170 }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        // Prefer automatic joystick data channel (HID profile active) over
        // manual input bindings — fall back to inputs.x/y for other controllers.
        var joystick = data && data.joystick;
        var rawX  = joystick ? joystick.pan  : (inputs.x !== undefined ? inputs.x  : 0.5);
        var rawY  = joystick ? joystick.tilt : (inputs.y !== undefined ? inputs.y  : 0.5);

        // sensitivity / deadzone are GLOBAL joystick settings, supplied by the
        // host through the joystick data channel (HID profile / I/O Manager).
        // Fall back to conservative defaults when no joystick channel is active.
        var dz  = (joystick && joystick.deadzone    !== undefined) ? joystick.deadzone    : 0.05;
        var sen = (joystick && joystick.sensitivity !== undefined) ? joystick.sensitivity : 1.0;
        var pH  = params.personHeight !== undefined ? params.personHeight : 170;

        // Tilt bias: 170 cm is neutral; person height shifts the setpoint at init.
        // Applied once at initialization rather than every tick.
        var tiltBias = (170 - pH) / 300 * 0.12;  // fraction of tilt range

        // Velocity from stick position: 0 at centre/deadzone, ±1 at extremes.
        function axisVelocity(v) {
            var delta = v - 0.5;
            if (Math.abs(delta) < dz) return 0.0;
            var sign  = delta > 0 ? 1 : -1;
            var scale = (Math.abs(delta) - dz) / (0.5 - dz);
            return sign * scale;
        }

        var xVel = axisVelocity(rawX);
        var yVel = axisVelocity(rawY);

        // dt approximation for 50 Hz timer (MasterTimer default).
        var dt = 0.02;

        // Shortest-path / no-wipe between scenes is handled host-side in the
        // target case (the aim point moves in floor space, a straight line);
        // free-fly below simply integrates the stick from centre.

        return fixtures.map(function(f) {
            var intent = {};

            if (f.hasPanTilt) {
                // TARGET MODE: when the scene has an aim target, the host moves the
                // shared aim point with the joystick and the scene's Aim look
                // re-aims EVERY head at it, so multiple heads stay converged on the
                // spot (per-fixture angle flying can't do that — equal angle steps
                // from different geometries diverge).  Emit no pan/tilt here so this
                // effect doesn't fight the Aim look; it just declares the scene a
                // followspot and carries followMode / personHeight.
                var hasTarget = f.sceneTarget && f.sceneTarget.pan !== undefined;
                if (hasTarget)
                    return intent;

                // FREE-FLY (no target): the operator flies this head's angle from
                // centre with the stick.  Coherent for a single head; with no shared
                // point there is nothing to converge on.
                var panKey  = "pan_"  + f.id;
                var tiltKey = "tilt_" + f.id;

                // Sentinel until the first stick move leaves the deadzone, so the
                // scene retains DMX control until the operator actually flies it.
                if (state[panKey] === undefined) {
                    state[panKey]  = null;
                    state[tiltKey] = null;
                }

                if (xVel !== 0.0 || yVel !== 0.0) {
                    if (state[panKey] === null) {
                        state[panKey]  = 0.5 * f.panRange;
                        state[tiltKey] = (0.5 + tiltBias) * f.tiltRange;
                    }
                    state[panKey]  = Math.min(f.panRange,  Math.max(0, state[panKey]  + xVel * f.panRange  * sen * dt));
                    state[tiltKey] = Math.min(f.tiltRange, Math.max(0, state[tiltKey] + yVel * f.tiltRange * sen * dt));
                }

                if (state[panKey] !== null) {
                    intent.pan  = state[panKey];
                    intent.tilt = state[tiltKey];
                }
            }

            // Position-only: deliberately no colour or dimmer in the intent, so
            // the scene's own Color/Dimmer palettes always retain control.
            return intent;
        });
    };

    return effect;
})()
