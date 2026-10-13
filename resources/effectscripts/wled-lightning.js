/*
  QLC+ Effect Script: WLED Lightning
  Every so often a stretch of the strip flashes white a few times in quick succession,
  over colour 2; intensity is how often it strikes.
  Written from what WLED's "Lightning" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Lightning";
    effect.description = "From WLED: bursts of lightning flashes";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Lightning";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var gap = WLED.period(seg.intensity, 8000, 600);        // between strikes
        var n = Math.floor(seg.now / gap);
        var t = seg.now - n * gap;
        var start = Math.floor(WLED.hash(n, 1) * len), size = 1 + Math.floor(WLED.hash(n, 2) * len);
        var flashes = 2 + Math.floor(WLED.hash(n, 3) * 4);
        var lit = false;
        for (var f = 0; f < flashes; f++) {
            var at = f * (60 + WLED.hash(n, 10 + f) * 120);
            if (t >= at && t < at + 30 + WLED.hash(n, 20 + f) * 40) lit = true;
        }
        seg.fill(seg.color(1));
        if (lit)
            for (var i = start; i < Math.min(len, start + size); i++)
                seg.setPixelColor(i, seg.palette ? paint(i) : 0xFFFFFF);
        return seg.out();
    };

    return effect;
})();
