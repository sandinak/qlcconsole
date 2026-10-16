/*
  QLC+ Effect Script library: WLED

  What a WLED effect written for qlcconsole needs. The ports are written
  from what each effect does on a device and WLED's description of it --
  not from WLED's code (WLED is EUPL-licensed; these are the app's own).
  Provided: a segment with WLED's own speed, intensity, palette and three
  colours, setPixelColor()/fill(), colorFromPalette(), FastLED-style
  integer helpers, and period()/hash()/noise()/hue() for timing, repeatable
  randomness, smooth flicker and colour-wheel hues. A port says
      effect.requires = ["wled"];
      effect.wled     = "Chase";        // WLED's name for it, so a look
                                        // with WLED's Chase runs this on
                                        // the fixtures that aren't WLED
  and its tick() paints the segment and returns it:

      var seg = WLED.segment(fixtures, inputs, palettes, params);
      ...seg.setPixelColor(i, seg.colorFromPalette(i, true, false, 0))...
      return seg.out();

  Colours are WLED's: 0xRRGGBB numbers. The three colours are the look's
  first three (palettes.look.colors) -- on a WLED lantern, its primary,
  secondary and tertiary colour channels. Mirror and reverse (params.option,
  as the WLED definitions' Effect Option channel) are applied by out(), the
  way WLED applies them to a segment, so an effect never sees them.

  A sound-reactive port also says effect.dataChannels = ["audio"], takes
  tick()'s data argument, and reads it through WLED.audio(data) (null with
  no audio input: it rests, dark). Those that fade, shift or remember their
  pixels the way WLED's do keep them in state: WLED.recall() at the start,
  WLED.keep() at the end.

  Palettes 0 and 2-5 are built from the three colours as WLED builds them;
  6 (Party) and 11 (Rainbow) are FastLED's. Any other is drawn as 4
  (Color Gradient) until its table is imported -- WLED.paletteKnown() says
  which.
*/
var WLED = (function() {
    var W = {};

    W.r = function(c) { return (c >> 16) & 0xFF; };
    W.g = function(c) { return (c >> 8) & 0xFF; };
    W.b = function(c) { return c & 0xFF; };
    W.rgb = function(r, g, b) { return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF); };

    // FastLED 8/16-bit maths, as integers.
    W.qadd8 = function(a, b) { return Math.min(255, a + b); };
    W.qsub8 = function(a, b) { return Math.max(0, a - b); };
    W.scale8 = function(i, s) { return (i * s) >> 8; };
    /** sin16(0..65535) -> -32767..32767 (FastLED's is a close approximation). */
    W.sin16 = function(x) { return Math.round(Math.sin((x & 0xFFFF) / 65536 * 2 * Math.PI) * 32767); };
    W.cos16 = function(x) { return W.sin16((x & 0xFFFF) + 16384); };
    /** sin8(0..255) -> 0..255. */
    W.sin8 = function(x) { return Math.round(128 + Math.sin((x & 0xFF) / 256 * 2 * Math.PI) * 127.5) & 0xFF; };
    W.cos8 = function(x) { return W.sin8((x & 0xFF) + 64); };
    W.triwave8 = function(x) { x &= 0xFF; if (x & 0x80) x = 255 - x; return (x << 1) & 0xFF; };
    W.beat8 = function(bpm, now) { return Math.floor(now * bpm * 256 / 60000) & 0xFF; };
    W.beat16 = function(bpm, now) { return Math.floor(now * bpm * 65536 / 60000) & 0xFFFF; };
    W.beatsin8 = function(bpm, lo, hi, now, phase) {
        lo = lo || 0; hi = (hi === undefined) ? 255 : hi;
        var s = W.sin8(W.beat8(bpm, now) + (phase || 0));
        return lo + W.scale8(s, hi - lo);
    };
    W.beatsin16 = function(bpm, lo, hi, now, phase) {
        lo = lo || 0; hi = (hi === undefined) ? 65535 : hi;
        var s = W.sin16(W.beat16(bpm, now) + (phase || 0)) + 32768;
        return lo + Math.floor(s * (hi - lo) / 65536);
    };

    /** WLED's color_blend(): @p blend 0 = all c1, 255 = all c2. */
    W.colorBlend = function(c1, c2, blend) {
        if (blend <= 0) return c1;
        if (blend >= 255) return c2;
        var mix = function(a, b) { return ((b * blend) + (a * (255 - blend))) >> 8; };
        return W.rgb(mix(W.r(c1), W.r(c2)), mix(W.g(c1), W.g(c2)), mix(W.b(c1), W.b(c2)));
    };
    /** WLED's color_fade(): scale toward black by @p amount/255. */
    W.colorFade = function(c, amount) {
        return W.rgb(W.scale8(W.r(c), amount), W.scale8(W.g(c), amount), W.scale8(W.b(c), amount));
    };
    /** WLED's color_wheel(): 0..255 round red, green, blue. */
    W.colorWheel = function(pos) {
        pos = 255 - (pos & 0xFF);
        if (pos < 85) return W.rgb(255 - pos * 3, 0, pos * 3);
        if (pos < 170) { pos -= 85; return W.rgb(0, pos * 3, 255 - pos * 3); }
        pos -= 170; return W.rgb(pos * 3, 255 - pos * 3, 0);
    };

    // A seeded random8 per segment (state.seed), so a port is repeatable.
    W.random8 = function(state, lim, hi) {
        state.seed = ((state.seed === undefined ? 1337 : state.seed) * 2053 + 13849) & 0xFFFF;
        var r = (state.seed + (state.seed >> 8)) & 0xFF;
        if (hi !== undefined) return lim + ((r * (hi - lim)) >> 8);
        if (lim !== undefined) return (r * lim) >> 8;
        return r;
    };
    W.random16 = function(state, lim) {
        var r = (W.random8(state) << 8) | W.random8(state);
        return lim !== undefined ? Math.floor(r * lim / 65536) : r;
    };

    var PARTY = [0x5500AB, 0x84007C, 0xB5004B, 0xE5001B, 0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
                 0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E, 0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9];
    var RAINBOW = [0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00, 0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
                   0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5, 0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B];

    /** CRGBPalette16(a, b, c): a gradient through the given stops. */
    function gradient16(stops) {
        var out = [];
        for (var k = 0; k < 16; k++) {
            var pos = k / 15 * (stops.length - 1);
            var i = Math.min(stops.length - 2, Math.floor(pos));
            out.push(W.colorBlend(stops[i], stops[i + 1], Math.round((pos - i) * 255)));
        }
        return out;
    }

    W.paletteKnown = function(id) { return id === 0 || (id >= 2 && id <= 6) || id === 11; };

    /** The sixteen entries of WLED palette @p id, from colours @p c. */
    W.palette16 = function(id, c) {
        var p = c[0], s = c[1], t = c[2];
        switch (id) {
        case 2: return [p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p];
        case 3: return [p, p, p, p, p, p, p, p, s, s, s, s, s, s, s, s];
        case 5: return [p, p, p, p, p, s, s, s, s, s, t, t, t, t, t, p];
        case 6: return PARTY;
        case 11: return RAINBOW;
        case 4:
        default: return gradient16([t, s, p]);
        }
    };

    /** FastLED ColorFromPalette() with linear blending between entries. */
    W.colorFromPalette16 = function(pal, index) {
        index &= 0xFF;
        var hi = index >> 4, lo = index & 0x0F;
        var a = pal[hi], b = pal[(hi + 1) & 15];
        return lo ? W.colorBlend(a, b, lo << 4) : a;
    };

    W.segment = function(fixtures, inputs, palettes, params) {
        var n = fixtures.length;
        var opt = Math.floor((params.option || 0) / 64);
        var mirror = (opt === 1 || opt === 3), reverse = (opt === 2 || opt === 3);
        var L = (palettes && palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var cols = [0, 1, 2].map(function(k) {
            var c = L[k];
            return (c && c.r !== undefined) ? W.rgb(Math.round(c.r), Math.round(c.g), Math.round(c.b)) : 0;
        });
        var seg = {
            len: mirror ? Math.floor((n + 1) / 2) : n,      // SEGLEN
            now: Math.floor(((inputs && inputs._time) || 0) * 1000),   // strip.now (ms)
            speed: Math.round(params.speed === undefined ? 128 : params.speed),
            intensity: Math.round(params.intensity === undefined ? 128 : params.intensity),
            palette: Math.round(params.palette || 0),
            solidWrap: false,               // PALETTE_SOLID_WRAP at WLED's default blend
            px: []
        };
        for (var i = 0; i < seg.len; i++) seg.px.push(0);
        var pal16 = seg.palette === 0 ? null : W.palette16(seg.palette, cols);

        seg.color = function(k) { return cols[k] || 0; };           // SEGCOLOR(k)
        seg.setPixelColor = function(i, c) { if (i >= 0 && i < seg.len) seg.px[i] = c; };
        seg.getPixelColor = function(i) { return (i >= 0 && i < seg.len) ? seg.px[i] : 0; };
        seg.fill = function(c) { for (var i = 0; i < seg.len; i++) seg.px[i] = c; };
        /** WLED's color_from_palette(): palette 0 is colour @p mcol itself. */
        seg.colorFromPalette = function(i, mapping, wrap, mcol) {
            if (pal16 === null && mcol < 3) return cols[mcol];
            var pal = pal16 || W.palette16(4, cols);
            var idx = i;
            if (mapping && seg.len > 1) idx = Math.floor((i * 255) / (seg.len - 1));
            if (!wrap) idx = W.scale8(idx, 240);
            return W.colorFromPalette16(pal, idx);
        };
        /** A "random" colour for @p u (0..1, from W.hash): off the palette, or
         *  round the colour wheel with none -- as WLED's random colours are. */
        seg.randomColor = function(u) {
            var k = Math.min(255, Math.max(0, Math.floor(u * 256)));
            return seg.palette ? seg.colorFromPalette(k, false, true, 0) : W.colorWheel(k);
        };
        /** One {r, g, b} per fixture: mirror and reverse as WLED does them. */
        seg.out = function() {
            var out = [];
            for (var j = 0; j < n; j++) {
                var v = (mirror && j >= seg.len) ? n - 1 - j : j;
                if (reverse) v = seg.len - 1 - v;
                var c = seg.px[v] || 0;
                out.push({ r: W.r(c), g: W.g(c), b: W.b(c) });
            }
            return out;
        };
        return seg;
    };

    /* ---- For ports written from what an effect DOES (not from WLED's code) ---- */

    /** How long one cycle lasts at WLED speed @p speed (0..255): @p slowMs at
     *  0 down to @p fastMs at 255, on a curve the ear and eye read as even. */
    W.period = function(speed, slowMs, fastMs) {
        var s = Math.max(0, Math.min(255, speed)) / 255;
        return slowMs * Math.pow(fastMs / slowMs, s);
    };
    /** A repeatable "random" 0..1 for (a, b): the same inputs, the same value,
     *  so an effect looks the same on every run and on every fixture that
     *  shares the moment (scripts must not use Math.random). */
    W.hash = function(a, b) {
        var x = Math.sin(a * 12.9898 + (b || 0) * 78.233 + 0.5) * 43758.5453;
        return x - Math.floor(x);
    };
    /** A hue 0..255 round the colour wheel, at saturation @p sat 0..255. */
    W.hue = function(h, sat) {
        var c = W.colorWheel(h & 0xFF);
        return sat === undefined || sat >= 255 ? c : W.colorBlend(0xFFFFFF, c, sat);
    };

    /** Smooth repeatable noise 0..1 along @p x (one bump per unit), a
     *  different curve for each @p seed: flicker, flames, drifting levels. */
    W.noise = function(x, seed) {
        var i = Math.floor(x), f = x - i;
        var a = W.hash(i, seed || 0), b = W.hash(i + 1, seed || 0);
        var u = f * f * (3 - 2 * f);
        return a + (b - a) * u;
    };

    /** @p c over @p bg at level @p lvl 0..1 (clamped): a pixel fading in or out. */
    W.mix = function(bg, c, lvl) {
        return W.colorBlend(bg, c, Math.round(Math.max(0, Math.min(1, lvl)) * 255));
    };
    /** @p c at @p lvl 0..1 of its brightness. */
    W.dim = function(c, lvl) {
        return W.colorFade(c, Math.round(Math.max(0, Math.min(1, lvl)) * 255));
    };
    /** @p a and @p b added, each channel capped at 255 (light on light). */
    W.add = function(a, b) {
        return W.rgb(W.qadd8(W.r(a), W.r(b)), W.qadd8(W.g(a), W.g(b)), W.qadd8(W.b(a), W.b(b)));
    };
    /** Where a ball thrown up from the floor at time 0 at @p v (pixels/s) is
     *  @p t s later under gravity @p g (pixels/s^2), each bounce keeping
     *  @p keep of its speed: { pos, vel }. Worked out, not stepped -- no state. */
    W.bounce = function(t, v, g, keep) {
        var guard = 0;
        while (v > 0.5 && guard++ < 64) {
            var flight = 2 * v / g;
            if (t < flight) return { pos: v * t - g * t * t / 2, vel: v - g * t };
            t -= flight;
            v *= keep;
        }
        return { pos: 0, vel: 0 };
    };
    /** Fire's colour at heat @p x 0..1: black, red, yellow, white. */
    W.heatColor = function(x) {
        var ch = function(v) { return Math.max(0, Math.min(255, Math.round(v * 3 * 255))); };
        return W.rgb(ch(x), ch(x - 1 / 3), ch(x - 2 / 3));
    };

    /* ---- Sound-reactive ports (effect.dataChannels = ["audio"]) ---- */

    /** What the host's audio channel says this tick, in the terms WLED's
     *  sound-reactive effects use: { vol, peak, bands[16] (0..1, low to high),
     *  major (the loudest band), majorHz, mag (its level), pitch (majorHz on
     *  a log scale, 0..1) } -- or null when no audio input is delivering, so
     *  an effect can rest. */
    W.audio = function(data) {
        var a = data && data.audio;
        if (!a) return null;
        var src = a.bands || [], b = [];
        for (var k = 0; k < 16; k++) {
            var v = src.length ? src[Math.min(src.length - 1, Math.floor(k * src.length / 16))] : 0;
            b.push(Math.max(0, Math.min(1, v || 0)));
        }
        var major = 0;
        for (var j = 1; j < 16; j++) if (b[j] > b[major]) major = j;
        var maxHz = a.maxHz || 5000, hz = (major + 0.5) / 16 * maxHz;
        return {
            vol: Math.max(0, Math.min(1, a.level || 0)),
            peak: Math.max(0, Math.min(1, a.peak || 0)),
            bands: b,
            major: major,
            majorHz: hz,
            mag: b[major],
            pitch: Math.max(0, Math.min(1, Math.log(Math.max(1, hz / 60)) / Math.log(maxHz / 60)))
        };
    };
    /** A beat: the volume jumping well above its recent average (kept in
     *  @p state), no more often than every 120 ms. Call it once a tick. */
    W.beat = function(state, au, now) {
        if (!au) return false;
        var first = state.beatAvg === undefined;
        var avg = first ? 0 : state.beatAvg;               // the first loud moment is a beat
        state.beatAvg = first ? au.vol : avg * 0.9 + au.vol * 0.1;
        var last = state.beatAt === undefined ? -1e9 : state.beatAt;
        if (au.vol > 0.08 && au.vol > avg * 1.35 && now - last > 120 && now >= last) {
            state.beatAt = now;
            return true;
        }
        return false;
    };
    /** The segment's pixels as they were left last tick (@p state), so a
     *  sound-reactive effect can fade, shift and add to them as WLED's do;
     *  sets seg's pixels from them and returns the ms since then. */
    W.recall = function(seg, state) {
        if (!state.px || state.px.length !== seg.len) {
            state.px = [];
            for (var i = 0; i < seg.len; i++) state.px.push(0);
        }
        var dt = state.t === undefined ? 0 : Math.max(0, Math.min(500, seg.now - state.t));
        state.t = seg.now;
        for (var j = 0; j < seg.len; j++) seg.px[j] = state.px[j];
        return dt;
    };
    /** Keep the segment's pixels for the next tick (see recall()). */
    W.keep = function(seg, state) {
        state.px = seg.px.slice();
    };
    /** How many whole @p stepMs steps have passed in @p dt ms, carrying the
     *  rest over in @p state: for effects that shift their pixels along. */
    W.steps = function(state, dt, stepMs) {
        state.stepAcc = (state.stepAcc || 0) + dt;
        var n = Math.floor(state.stepAcc / stepMs);
        state.stepAcc -= n * stepMs;
        return n;
    };
    /** Move every pixel one place toward the start (@p dir < 0), the end
     *  (@p dir > 0) or out from the middle (@p dir 0). */
    W.shift = function(seg, dir) {
        var len = seg.len, px = seg.px, i;
        if (dir < 0) for (i = 0; i < len - 1; i++) px[i] = px[i + 1];
        else if (dir > 0) for (i = len - 1; i > 0; i--) px[i] = px[i - 1];
        else {
            var mid = Math.floor((len - 1) / 2);
            for (i = 0; i < mid; i++) px[i] = px[i + 1];
            for (i = len - 1; i > mid + 1; i--) px[i] = px[i - 1];
        }
    };
    /** Fade every pixel toward black: @p perSecond 0..1 of the light gone a
     *  second, over @p dt ms. */
    W.fadeOut = function(seg, perSecond, dt) {
        var k = Math.pow(Math.max(0, 1 - Math.min(0.999, perSecond)), dt / 1000);
        for (var i = 0; i < seg.len; i++) seg.px[i] = W.dim(seg.px[i], k);
    };

    /** The parameters every port takes: WLED's own, 0..255 as the definitions
     *  send them. */
    W.parameters = function() {
        return [
            { name: "speed",     description: "Speed (WLED)",     min: 0, max: 255, defaultValue: 128 },
            { name: "intensity", description: "Intensity (WLED)", min: 0, max: 255, defaultValue: 128 },
            { name: "palette",   description: "Palette (WLED number, 0 = colours)", min: 0, max: 71, defaultValue: 0 },
            { name: "option",    description: "Option (64 mirror, 128 reverse, 192 both)", min: 0, max: 255, defaultValue: 0 }
        ];
    };

    return W;
})();
