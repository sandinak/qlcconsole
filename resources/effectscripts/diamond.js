/*
  QLC+ Effect Script: Diamond
  Each fixture traces a diamond (a square rotated 45°) in pan/tilt — the stock
  EFX "Diamond" algorithm as a composable script. Drives pan/tilt only; add a
  Colour/Dimmer look to the scene for those.
*/
(function() {
    var effect = new Object;
    effect.apiVersion   = 1;
    effect.name         = "Diamond";
    effect.description  = "Diamond (rotated-square) pan/tilt path";
    effect.author       = "QLC+";
    effect.fixtureTypes = ["moving"];
    effect.notes = "Beams trace a diamond outline (a square rotated 45°). This is the stock EFX 'Diamond' algorithm as a composable effect — keep EFX for legacy shows; use this to lay diamond movement over Colour/Dimmer looks. Phase spread staggers fixtures around the diamond so they chase its edges. Supports beat sync (one lap per N beats).";

    effect.inputs = [
        { name: "x", description: "Diamond centre pan  (0=left, 0.5=mid, 1=right)", defaultValue: 0.5 },
        { name: "y", description: "Diamond centre tilt (0=up,   0.5=mid, 1=down)",  defaultValue: 0.5 }
    ];

    effect.parameters = [
        { name: "speed",       description: "Laps per second",                     min: -3.0, max: 3.0, defaultValue: 0.3 },
        { name: "panRadius",   description: "Pan half-width (fraction of range)",   min: 0.0,  max: 0.5, defaultValue: 0.25 },
        { name: "tiltRadius",  description: "Tilt half-width (fraction of range)",  min: 0.0,  max: 0.5, defaultValue: 0.2 },
        { name: "phaseSpread", description: "Phase spread across fixtures (0=clump, 1=full lap)", min: 0.0, max: 1.0, defaultValue: 0.8 },
        { name: "beatSync",    description: "Laps per beat (0=off, 1=1/beat)",      min: 0.0, max: 4.0, defaultValue: 0.0 }
    ];

    // Project a circle angle onto a diamond outline (|x|+|y| = const): dividing
    // cos/sin by (|cos|+|sin|) maps the unit circle to a unit diamond.
    function diamond(theta) {
        var c = Math.cos(theta), s = Math.sin(theta);
        var d = Math.abs(c) + Math.abs(s);
        if (d < 1e-6) return { x: 0, y: 0 };
        return { x: c / d, y: s / d };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t         = inputs._time      !== undefined ? inputs._time      : 0;
        var beat      = inputs._beat      !== undefined ? inputs._beat      : 0;
        var bpm       = inputs._bpm       !== undefined ? inputs._bpm       : 0;
        var beatCount = inputs._beatCount !== undefined ? inputs._beatCount : 0;
        var cx        = inputs.x          !== undefined ? inputs.x          : 0.5;
        var cy        = inputs.y          !== undefined ? inputs.y          : 0.5;

        var speed       = params.speed       !== undefined ? params.speed       : 0.3;
        var panRadius   = params.panRadius   !== undefined ? params.panRadius   : 0.25;
        var tiltRadius  = params.tiltRadius  !== undefined ? params.tiltRadius  : 0.2;
        var phaseSpread = params.phaseSpread !== undefined ? params.phaseSpread : 0.8;
        var beatSync    = params.beatSync    !== undefined ? params.beatSync    : 0.0;

        var n = Math.max(fixtures.length, 1);

        var baseTheta;
        if (beatSync > 0 && bpm > 0)
            baseTheta = (beatCount + beat) * beatSync * 2 * Math.PI;
        else
            baseTheta = t * speed * 2 * Math.PI;

        return fixtures.map(function(f, i) {
            if (!f.hasPanTilt) return {};

            var fixturePhase = (n > 1) ? (i / (n - 1)) * phaseSpread * 2 * Math.PI : 0;
            var d = diamond(baseTheta - fixturePhase);

            var pan  = (cx + panRadius  * d.x) * f.panRange;
            var tilt = (cy + tiltRadius * d.y) * f.tiltRange;

            var intent = {
                pan:  Math.max(0, Math.min(f.panRange,  pan)),
                tilt: Math.max(0, Math.min(f.tiltRange, tilt))
            };

            if (palettes.dimmer && palettes.dimmer.dimmer !== undefined)
                intent.dimmer = palettes.dimmer.dimmer;
            if (palettes.color && palettes.color.r !== undefined) {
                intent.r = palettes.color.r; intent.g = palettes.color.g; intent.b = palettes.color.b;
            }
            return intent;
        });
    };

    return effect;
})()
