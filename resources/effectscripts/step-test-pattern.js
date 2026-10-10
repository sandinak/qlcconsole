/*
  QLC+ Effect Script: Step Test Pattern (diagnostic)

  A test card for a rig of LED-strip steps named the usual way: each step's
  front strips "<step> #<n>" ("DS1 #3": step DS1, its third strip), its top
  bars "<step>t<n>" ("CS2t1"). On every step's front:

    - strip n has its first n LEDs white (strip 1: one, strip 2: two ...),
      so the order of the strips and the end each counts from can be read;
    - the last LED of every strip is white, so a strip's length can be checked;
    - from the 10th LED, a vertical line per step number (DS1: one, CS3:
      three), every other LED -- blue for DS, green for CS, red for US;
    - the step's name in the middle, white, and beside it a spinner as tall
      as the step going round once a second -- so a frozen output shows at a
      glance.

  On every top bar, a blue dot runs back and forth at a low level.

  Which strip is on top is taken from where the strips are on the rig (the
  highest first); where they are all at one height, strip 1 is the top.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Step Test Pattern";
    effect.description = "Diagnostic: each step's strips counted, numbered and named; a dot on the tops";
    effect.author      = "qlcconsole";
    effect.fixtureTypes = ["rgb"];
    effect.notes = "For steps whose front strips are named \"<step> #<n>\" (DS1 #3) and top bars \"<step>t<n>\" (CS2t1). Strip n lights its first n LEDs white and every strip its last; from LED 10, one line per step number (DS blue, CS green, US red); the step name in the middle with a spinner beside it (once round a second: still means the output is frozen); a blue dot on the tops. If the name reads backwards, the strips count from the other end: use Mirror.";

    effect.inputs = [];
    effect.palettes = [];
    effect.parameters = [
        { name: "brightness", description: "Level of the front pattern (0-1)", min: 0.0, max: 1.0, defaultValue: 0.6 },
        { name: "dotLevel",   description: "Level of the dot on the tops (0-1)", min: 0.0, max: 1.0, defaultValue: 0.25 },
        { name: "speed",      description: "Dot speed, pixels a second",        min: 1.0, max: 40.0, defaultValue: 8.0 },
        { name: "topPixels",  description: "Colour pixels at the start of a top bar", min: 1, max: 64, defaultValue: 16 },
        { name: "mirror",     description: "Count the front from the other end (0/1)", min: 0, max: 1, defaultValue: 0 }
    ];

    // A 5 x 7 font: what a step name needs.
    var FONT = {
        "C": ["01110","10001","10000","10000","10000","10001","01110"],
        "D": ["11110","10001","10001","10001","10001","10001","11110"],
        "U": ["10001","10001","10001","10001","10001","10001","01110"],
        "S": ["01111","10000","10000","01110","00001","00001","11110"],
        "0": ["01110","10001","10011","10101","11001","10001","01110"],
        "1": ["00100","01100","00100","00100","00100","00100","01110"],
        "2": ["01110","10001","00001","00010","00100","01000","11111"],
        "3": ["11111","00010","00100","00010","00001","10001","01110"],
        "4": ["00010","00110","01010","10010","11111","00010","00010"],
        "5": ["11111","10000","11110","00001","00001","10001","01110"],
        "6": ["00110","01000","10000","11110","10001","10001","01110"],
        "7": ["11111","00001","00010","00100","01000","01000","01000"],
        "8": ["01110","10001","10001","01110","10001","10001","01110"],
        "9": ["01110","10001","10001","01111","00001","00010","01100"]
    };
    var GLYPH_W = 5, GLYPH_H = 7, GAP = 1;

    function textPixel(text, col, row, cols) {
        var width = text.length * GLYPH_W + (text.length - 1) * GAP;
        var x0 = Math.floor((cols - width) / 2);
        var x = col - x0;
        if (x < 0 || x >= width || row < 0 || row >= GLYPH_H) return false;
        var ch = Math.floor(x / (GLYPH_W + GAP));
        var gx = x - ch * (GLYPH_W + GAP);
        if (gx >= GLYPH_W) return false;
        var g = FONT[text.charAt(ch)];
        return g !== undefined && g[row].charAt(gx) === "1";
    }

    // The spinner: a ring beside the name as tall as the step (every strip)
    // and as wide, a dot going round it once a second with a fading tail.
    var SPIN_GAP = 2;                    // columns after the name
    var TAIL = [1.0, 0.5, 0.25, 0.1];

    // Where cell (x, y) is on the edge of a w x h ring, clockwise from the
    // top left; -1 inside it.
    function ringIndex(x, y, w, h) {
        if (y === 0) return x;
        if (x === w - 1) return (w - 1) + y;
        if (y === h - 1) return (w - 1) + (h - 1) + (w - 1 - x);
        if (x === 0) return 2 * (w - 1) + (h - 1) + (h - 1 - y);
        return -1;
    }

    // How lit cell (col, row) of the spinner is at time t, 0..1. @rows: the
    // step's strips.
    function spinnerLevel(text, col, row, cols, rows, t) {
        var width = text.length * GLYPH_W + (text.length - 1) * GAP;
        var x0 = Math.floor((cols - width) / 2) + width + SPIN_GAP;
        var h = Math.max(3, rows);
        var w = Math.max(3, Math.min(h, cols - 1 - x0));   // never the last LED
        var x = col - x0, y = row;
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        var i = ringIndex(x, y, w, h);
        if (i < 0) return 0;
        var n = 2 * (w - 1) + 2 * (h - 1);
        var at = Math.floor(t * n) % n;
        var back = (at - i + n) % n;
        return back < TAIL.length ? TAIL[back] : 0;
    }

    // What a fixture's name says: a front strip, a top bar, or neither.
    function parseName(name) {
        var m = /^([A-Za-z]+)(\d+)\s*#(\d+)/.exec(name);
        if (m) return { front: true, step: m[1].toUpperCase() + m[2],
                        num: parseInt(m[2], 10), strip: parseInt(m[3], 10) };
        if (/^([A-Za-z]+)(\d+)t(\d+)$/i.test(name)) return { top: true };
        return {};
    }

    function sectionColour(step, level) {
        var v = Math.round(255 * level);
        switch (step.charAt(0)) {
            case "U": return { r: v, g: 0, b: 0 };
            case "C": return { r: 0, g: v, b: 0 };
            default:  return { r: 0, g: 0, b: v };
        }
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var t      = inputs._time !== undefined ? inputs._time : 0;
        var level  = params.brightness !== undefined ? params.brightness : 0.6;
        var dot    = params.dotLevel   !== undefined ? params.dotLevel   : 0.25;
        var speed  = params.speed      !== undefined ? params.speed      : 8.0;
        var topPx  = Math.max(1, Math.round(params.topPixels !== undefined ? params.topPixels : 16));
        var mirror = (params.mirror !== undefined ? params.mirror : 0) >= 0.5;
        var white  = Math.round(255 * level);
        var off    = { r: 0, g: 0, b: 0 };

        // What each name says, worked out once a name, not once a pixel.
        var names = state.names || (state.names = {});
        function info(f) {
            var name = f.name || "";
            return names[name] || (names[name] = parseName(name));
        }

        // Each step's strips, top first: by height on the rig, else by number.
        var strips = {};
        fixtures.forEach(function(f) {
            var p = info(f);
            if (!p.front) return;
            var s = strips[p.step] || (strips[p.step] = {});
            if (s[f.id] === undefined)
                s[f.id] = { n: p.strip, z: (f.pos && f.pos.z !== undefined) ? f.pos.z : 0 };
        });
        var rowOf = {};       // fixture id -> row from the top
        var stepRows = {};    // step -> its strips
        for (var step in strips) {
            var list = [];
            for (var id in strips[step]) list.push({ id: id, s: strips[step][id] });
            list.sort(function(a, b) {
                if (Math.abs(a.s.z - b.s.z) > 0.01) return b.s.z - a.s.z;   // higher first
                return a.s.n - b.s.n;
            });
            list.forEach(function(e, i) { rowOf[e.id] = i; });
            stepRows[step] = list.length;
        }

        // The dot: there and back across the top's colour pixels.
        var span = Math.max(1, topPx - 1);
        var travel = (t * speed) % (2 * span);
        var dotAt = travel <= span ? travel : 2 * span - travel;

        return fixtures.map(function(f) {
            var p = info(f);
            if (p.front) {
                var step  = p.step;
                var num   = p.num;
                var strip = p.strip;
                var cols  = f.heads || 64;
                var led   = mirror ? (cols - 1 - f.head) : f.head;
                if (led < strip || led === cols - 1)
                    return { r: white, g: white, b: white };
                // One line per step number from the 10th LED, every other one.
                if (led >= 9 && led < 9 + 2 * num && (led - 9) % 2 === 0)
                    return sectionColour(step, level);
                if (textPixel(step, led, rowOf[f.id], cols))
                    return { r: white, g: white, b: white };
                var spin = spinnerLevel(step, led, rowOf[f.id], cols, stepRows[step], t);
                if (spin > 0)
                    return sectionColour(step, level * spin);
                return off;
            }
            if (p.top) {
                if (f.head >= topPx) return off;            // its white pixels
                var d = Math.abs(f.head - dotAt);
                var k = d < 0.5 ? 1.0 : d < 1.5 ? 0.35 : d < 2.5 ? 0.12 : 0.0;
                return { r: 0, g: 0, b: Math.round(255 * dot * k) };
            }
            return off;
        });
    };

    return effect;
})()
