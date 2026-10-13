/*
  QLC+ Effect Script: WLED Halloween Eyes
  Pairs of eyes in colour 1 open at random places on the strip, stare, blink and close.
  Written from what WLED's "Halloween Eyes" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Halloween Eyes";
    effect.description = "From WLED: pairs of eyes opening, blinking and closing";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Halloween Eyes";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var life = WLED.period(seg.speed, 6000, 1500);
        var gap = 2;
        seg.fill(seg.color(1));
        var n = Math.floor(seg.now / life), t = (seg.now % life) / life;
        var at = Math.floor(WLED.hash(n, 1) * Math.max(1, len - gap - 1));
        var lvl = t < 0.1 ? t / 0.1 : t > 0.85 ? (1 - t) / 0.15 : 1;
        var blink = Math.abs(t - 0.5) < 0.03;
        if (!blink) {
            var c = WLED.colorBlend(0, paint(at), Math.round(lvl * 255));
            seg.setPixelColor(at, c);
            seg.setPixelColor(at + gap + 1, c);
        }
        return seg.out();
    };

    return effect;
})();
