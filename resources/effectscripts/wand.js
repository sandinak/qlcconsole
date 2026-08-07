/*
  QLC+ Effect Script: Wand
  A bright head sweeps across the pixels ONCE, leaving a fading trail, then
  clears — a one-shot with a natural life cycle. It rides inputs.phase (0→1), so
  it fits whatever run length the cue/step (or a per-look override) gives it,
  landing in time with the show. See EFFECT_LIFECYCLE_DESIGN.md.

  Pure & testable: position is inputs.phase, not wall-clock.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Wand";
    effect.description = "A bright wand sweeps across the pixels once, then clears";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];

    // Lifecycle: one pass over the resolved duration, then stop driving.
    effect.lifecycle        = "oneshot";
    effect.syncTo           = "span";      // fit the cue/step length by default
    effect.onFinish         = "release";   // clear after the sweep (a transient)
    effect.naturalDurationMs = 1500;       // fallback length if nothing else resolves

    effect.notes = "A one-shot: a bright head sweeps across the fixtures a single time over the effect's duration, leaving a fading tail, then clears. The length follows the cue/chase step (or a per-look override); Trail sets the tail; Ease fades the wand in at the start and out at the end. Colour comes from the look's first Colour palette.";

    effect.parameters = [
        { name: "trail", description: "Trail length (cells)", min: 1, max: 40, defaultValue: 8 },
        { name: "ease",  description: "Ease in/out at the ends", defaultValue: 1, values: ["Off", "On"] }
    ];

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n = fixtures.length;
        if (n === 0) return [];

        var g0    = fixtures[0].grid || { cols: n, rows: 1 };
        var cols  = g0.cols || n, rows = g0.rows || 1;
        var total = Math.max(1, cols * rows);

        // One pass across the flattened grid, driven by the lifecycle phase.
        var phase = (inputs.phase !== undefined) ? inputs.phase : 0;
        var head  = phase * (total - 1);
        var trail = Math.max(1, params.trail || 8);

        // Optional envelope so the wand grows in and fades out at the extremes.
        var env = 1;
        if ((params.ease | 0) === 1) env = Math.sin(Math.PI * phase);   // 0 at ends, 1 mid

        var L    = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var col0 = (L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 };

        return fixtures.map(function(f) {
            var g   = f.grid || { col: 0, row: 0, cols: cols, rows: rows };
            var idx = g.col + g.row * cols;
            var d   = head - idx;                          // trail sits BEHIND the head
            var b   = (d >= 0 && d < trail) ? (1 - d / trail) : 0;
            b *= env;
            var out = { r: Math.round(col0.r * b), g: Math.round(col0.g * b), b: Math.round(col0.b * b) };
            if (f.hasDimmer) out.dimmer = b;
            return out;
        });
    };

    return effect;
})()
