/*
  QLC+ Effect Script: WLED Fairytwinkle
  Colortwinkle the other way round: every pixel lit in its colour, each now and
  then fading out and back in; intensity is how many twinkle.
  Written from what WLED's "Fairytwinkle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fairytwinkle";
    effect.description = "From WLED: all lit, pixels fading out and back at random";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fairytwinkle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var base = WLED.period(seg.speed, 8000, 800);
        var share = 0.05 + seg.intensity / 255 * 0.6;            // the share that twinkle each round
        for (var i = 0; i < len; i++) {
            var rate = base * (0.7 + 0.6 * WLED.hash(i, 90));
            var t = (seg.now + WLED.hash(i, 91) * rate) / rate;
            var n = Math.floor(t), ph = t - n;
            var lvl = WLED.hash(i, n + 92) < share ? 1 - Math.sin(ph * Math.PI) : 1;
            seg.setPixelColor(i, WLED.mix(seg.color(1), seg.randomColor(WLED.hash(i, 94)), lvl));
        }
        return seg.out();
    };

    return effect;
})();
