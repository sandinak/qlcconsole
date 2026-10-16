/*
  QLC+ Effect Script: WLED Dissolve Rnd
  Dissolve in random colours: the pixels light one by one in random order, each a
  random colour, then go out again the same way.
  Written from what WLED's "Dissolve Rnd" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Dissolve Rnd";
    effect.description = "From WLED: pixels lit at random in random colours, then out";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Dissolve Rnd";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var per = WLED.period(seg.intensity, 400, 15);          // a pixel each
        var hold = WLED.period(seg.speed, 3000, 200);           // a pause when full or empty
        var fill = len * per;
        var cycle = 2 * (fill + hold);
        var t = seg.now % cycle, round = Math.floor(seg.now / cycle);
        var on, done;
        if (t < fill) { on = true; done = t / per; }
        else if (t < fill + hold) { on = true; done = len; }
        else if (t < 2 * fill + hold) { on = false; done = (t - fill - hold) / per; }
        else { on = false; done = len; }
        for (var i = 0; i < len; i++) {
            var lit = WLED.hash(i, round + 0.5) * len < done;
            var isOn = on ? lit : !lit;
            seg.setPixelColor(i, isOn ? seg.randomColor(WLED.hash(i, round * 7 + 3)) : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
