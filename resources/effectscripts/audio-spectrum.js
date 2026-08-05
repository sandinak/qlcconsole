/*
  QLC+ Effect Script: Audio Spectrum
  Maps the live audio spectrum across the fixtures — low frequencies at one end,
  highs at the other — each fixture lit by its band's magnitude. On a matrix,
  columns are the spectrum and each column rises with its band (bar-graph).

  Subscribes to the host "audio" data channel (data.audio.bands[]/level/peak).
  Needs an audio input assigned + granted (Inputs/Outputs). Colour: a low→high
  hue ramp, or the look's palette.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Audio Spectrum";
    effect.description = "Fixtures light with the live audio frequency spectrum";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.dataChannels = ["audio"];
    effect.notes = "The audio spectrum spread across the fixtures (low→high). On a matrix each column is a frequency band rising bar-graph style; on a strip each fixture is a band. Gain scales the response; Smooth eases the fall. Set Start/End Hz to spread only part of the spectrum across the fixtures — e.g. 40–250 Hz to give a whole panel to the bass. Colours: Spectrum (low=red→high=blue) or the look's palette. Requires an audio input in Inputs/Outputs.";

    effect.parameters = [
        { name: "gain",    description: "Response gain",           min: 0.2, max: 5.0, defaultValue: 1.4 },
        { name: "smooth",  description: "Fall smoothing (0-0.95)", min: 0.0, max: 0.95, defaultValue: 0.6 },
        { name: "startHz", description: "Start Hz (0 = lowest)",   min: 0, max: 5000, defaultValue: 0 },
        { name: "endHz",   description: "End Hz (5000 = highest)", min: 0, max: 5000, defaultValue: 5000 },
        { name: "colors",  description: "Colour source",           defaultValue: 0, values: ["Spectrum", "Look palette"] }
    ];

    function hsv(h) {
        h = h - Math.floor(h);
        var i = Math.floor(h * 6), f = h * 6 - i, q = 1 - f, t = f, r, g, b;
        switch (i % 6) {
            case 0:  r = 1; g = t; b = 0; break;
            case 1:  r = q; g = 1; b = 0; break;
            case 2:  r = 0; g = 1; b = t; break;
            case 3:  r = 0; g = q; b = 1; break;
            case 4:  r = t; g = 0; b = 1; break;
            default: r = 1; g = 0; b = q; break;
        }
        return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var n = fixtures.length;
        if (n === 0) return [];

        var g0   = fixtures[0].grid || { cols: n, rows: 1 };
        var cols = g0.cols || n, rows = g0.rows || 1;

        var a     = data && data.audio;
        var bands = (a && a.bands) ? a.bands : [];
        var nb    = bands.length;
        var gain  = params.gain || 1.4;

        // Focus band window from the Hz range (bands tile 0..maxHz linearly).
        var maxHz = (a && a.maxHz) ? a.maxHz : 5000;
        var startHz = Math.max(0, params.startHz | 0);
        var endHz   = params.endHz ? (params.endHz | 0) : maxHz;
        if (endHz <= startHz) endHz = maxHz;
        var bLo = nb ? Math.max(0, Math.floor(startHz / maxHz * nb)) : 0;
        var bHi = nb ? Math.min(nb, Math.ceil(endHz / maxHz * nb)) : 0;
        if (bHi <= bLo) bHi = Math.min(nb, bLo + 1);
        var focusN = bHi - bLo;   // bands spread across the fixtures
        var smooth = Math.min(0.95, Math.max(0, params.smooth || 0.6));
        var useLook = ((params.colors | 0) === 1);
        var lk = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];

        // Smoothed per-column band values (fast attack, eased release).
        if (!state.col || state.col.length !== cols) {
            state.col = []; for (var c = 0; c < cols; c++) state.col[c] = 0;
        }
        for (var c = 0; c < cols; c++) {
            // Spread only the focused band window [bLo, bHi) across the columns.
            var bi = focusN > 0 ? Math.min(bHi - 1, bLo + Math.floor(c / cols * focusN)) : 0;
            var v = nb > 0 ? Math.min(1, bands[bi] * gain) : 0;
            state.col[c] = (v > state.col[c]) ? v : (state.col[c] * smooth + v * (1 - smooth));
        }

        return fixtures.map(function(f, idxFix) {
            var gg  = f.grid || { col: idxFix % cols, row: 0 };
            var col = gg.col, row = gg.row;
            var mag = state.col[col] || 0;

            var b;
            if (rows > 1) {
                // Bar graph: a column cell lights only up to its band height.
                var level = 1 - (row / (rows - 1 || 1));   // bottom row = 1
                b = (level <= mag) ? mag : 0;
            } else {
                b = mag;                                    // strip: each fixture = its band
            }

            var hue = cols > 1 ? (col / (cols - 1)) * 0.7 : 0;   // low=red(0)→high=blue(0.7)
            var cc  = useLook && lk.length
                        ? lk[Math.min(lk.length - 1, Math.floor(col / cols * lk.length))]
                        : hsv(hue);
            var out = { r: Math.round(cc.r * b), g: Math.round(cc.g * b), b: Math.round(cc.b * b) };
            if (f.hasDimmer) out.dimmer = b;
            return out;
        });
    };

    return effect;
})()
