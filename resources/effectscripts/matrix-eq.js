/*
  QLC+ Effect Script: Matrix EQ
  A graphic equaliser on the matrix: a bar per column (or per few) rising
  and falling with the music, peak dots falling slowly above them. Reads
  the live audio input when there is one; without, it plays a convincing
  one of its own, bouncing on the beat.

  Pure & testable: driven by inputs/data; the simulation from a hash, not
  Math.random. The peak dots are kept in state.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Matrix EQ";
    effect.description = "Equaliser bars with peak dots: live audio, or simulated without";
    effect.author      = "qlcconsole";
    effect.requires    = ["matrix"];
    effect.fixtureTypes = ["matrix", "rgb", "dimmer"];
    effect.dataChannels = ["audio"];
    effect.notes = "Bars rise and fall like a graphic equaliser, low frequencies on the left. Source: the live audio input (Inputs/Outputs) -- simulated, on the beat, while there is none -- or always simulated. Bar width: pixels a bar takes; Gain: how high the bars reach; Peaks: dots that hold the top of each bar and fall slowly (the accent: white on a box with white pixels). From: bars from the bottom, or from the middle out both ways. Colours: green to red up the bar, the look's colours up the bar, the look's colours across the bands, or a rainbow across. Scope: an equaliser in each box, or one across the whole canvas.";

    effect.parameters = [
        { name: "source",   description: "What drives the bars", defaultValue: 0, values: ["Audio input (simulated without)", "Simulated"] },
        { name: "barWidth", description: "Pixels a bar takes", min: 1, max: 8, defaultValue: 1 },
        { name: "gain",     description: "How high the bars reach", min: 0.2, max: 3, defaultValue: 1 },
        { name: "peaks",    description: "Peak dots: how long they hold (seconds; 0 = none)", min: 0, max: 2, defaultValue: 0.6 },
        { name: "from",     description: "Where the bars grow from", defaultValue: 0, values: ["Bottom", "Middle"] },
        { name: "colours",  description: "Colouring", defaultValue: 0,
          values: ["Green to red", "Look colours up the bar", "Look colours across", "Rainbow across"] },
        { name: "scope",    description: "An equaliser in each box, or one across the canvas", defaultValue: 1, values: MATRIX.SCOPES }
    ].concat(MATRIX.whiteParams(1));

    var GYR = [{ r: 0, g: 255, b: 0 }, { r: 255, g: 220, b: 0 }, { r: 255, g: 0, b: 0 }];

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        if (fixtures.length === 0) return [];
        var cv = MATRIX.canvas(fixtures);
        var num = MATRIX.num;
        var t = num(inputs._time, 0);
        var bw = Math.max(1, Math.round(num(params.barWidth, 1)));
        var gain = Math.max(0.05, num(params.gain, 1));
        var hold = Math.max(0, num(params.peaks, 0.6));
        var middle = Math.round(num(params.from, 0)) === 1;
        var scheme = Math.round(num(params.colours, 0));
        var perBox = Math.round(num(params.scope, 1)) === 1;
        var C = MATRIX.colours(palettes, GYR);
        var audio = (Math.round(num(params.source, 0)) === 0 && data && data.audio && data.audio.bands)
                    ? data.audio.bands : null;
        var bpm = num(inputs._bpm, 0);
        var beat = bpm > 0 ? num(inputs._beat, 0) : (t * 2) - Math.floor(t * 2);

        // A band's level 0..1 (16 bands, low to high).
        function band(u) {
            var i = MATRIX.clamp(Math.floor(u * 16), 0, 15);
            if (audio) return MATRIX.clamp(num(audio[i], 0) * gain, 0, 1);
            var x = i / 15;
            var kick = Math.pow(1 - beat, 3) * (1 - x) * 0.9;     // the beat, in the lows
            var wob = MATRIX.noise(t * (1.5 + x * 4), i * 3.7, 21);
            var level = 0.15 + 0.55 * wob * (1 - 0.35 * x) + kick * 0.6;
            return MATRIX.clamp(level * gain, 0, 1);
        }

        // Peak dots: held, then falling at a canvas-height a second.
        if (state.peaks === undefined) state.peaks = {};
        var dt = state.lastT === undefined ? 0 : Math.max(0, t - state.lastT);
        state.lastT = t;
        var seen = {};

        return fixtures.map(function(f) {
            var p = MATRIX.point(f, cv, perBox);
            var bars = Math.max(1, Math.ceil(p.w / bw));
            var bar = Math.min(bars - 1, Math.floor(p.x / bw));
            var u = bars > 1 ? bar / (bars - 1) : 0.5;
            var lvl = band(u);
            // Height from the bottom (or out from the middle), 0..1 at this pixel.
            var hgt;
            if (middle) {
                var c = (p.h - 1) / 2;
                hgt = p.h > 1 ? Math.abs(p.y - c) / Math.max(0.5, c + 0.5) : 0;
            } else {
                hgt = p.h > 1 ? (p.h - 1 - p.y) / p.h : 0;
            }
            var step = 1 / Math.max(1, middle ? Math.ceil(p.h / 2) : p.h);
            var on = lvl > hgt ? Math.min(1, (lvl - hgt) / step) : 0;

            // Its peak: one per bar per box.
            var key = (perBox && p.box ? p.box.index : 0) + ":" + bar;
            var pk = state.peaks[key];
            if (!seen[key]) {
                seen[key] = true;
                if (pk === undefined) pk = state.peaks[key] = { v: lvl, age: 0 };
                if (lvl >= pk.v) { pk.v = lvl; pk.age = 0; }
                else {
                    pk.age += dt;
                    if (pk.age > hold) pk.v = Math.max(lvl, pk.v - dt);
                }
            }
            var acc = 0;
            if (hold > 0 && pk.v > step && Math.abs(pk.v - hgt - step / 2) < step / 2 && on < 1)
                acc = 1;

            var colour;
            if (scheme === 1) colour = MATRIX.ramp(C, hgt, false);
            else if (scheme === 2) colour = MATRIX.ramp(C, u, false);
            else if (scheme === 3) colour = MATRIX.hue(u * 0.8);
            else colour = MATRIX.ramp(GYR, hgt, false);
            return MATRIX.out(f, cv, colour, on, acc, params);
        });
    };

    return effect;
})()
