/*
  QLC+ Effect Script: WLED Twinklefox
  Every pixel twinkles on its own: fading up and down at its own pace, in colours from
  the palette (or colour 1), over colour 2. Intensity is how many twinkle at once.
  Written from what WLED's "Twinklefox" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Twinklefox";
    effect.description = "From WLED: pixels twinkling softly at their own pace";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Twinklefox";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var base = WLED.period(seg.speed, 6000, 600);
        var density = 0.15 + seg.intensity / 255 * 0.85;
        seg.fill(seg.color(1));
        for (var i = 0; i < len; i++) {
            if (WLED.hash(i, 1) > density) continue;
            var rate = base * (0.6 + 0.8 * WLED.hash(i, 2));
            var t = (seg.now + WLED.hash(i, 3) * rate) / rate;
            var cycle = Math.floor(t), ph = t - cycle;
            var lvl = ph < 0.5 ? Math.sin(ph * Math.PI) : 0;    // up and down, then rest
            var c = seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(i, cycle) * 255), false, true, 0) : seg.color(0);
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), c, Math.round(lvl * 255)));
        }
        return seg.out();
    };

    return effect;
})();
