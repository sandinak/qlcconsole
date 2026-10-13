/*
  QLC+ Effect Script: WLED Dissolve
  Pixels switch to colour 1 one at a time at random until the strip is full, then switch
  back to colour 2 the same way; intensity is how fast they go.
  Written from what WLED's "Dissolve" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Dissolve";
    effect.description = "From WLED: pixels dissolve to the colour and back at random";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Dissolve";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var per = WLED.period(seg.intensity, 400, 15);           // a pixel each
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
            // The order this pixel goes in, this round (the same pixel order both ways).
            var rank = WLED.hash(i, round);
            var lit = rank * len < done;
            var isOn = on ? lit : !lit;
            seg.setPixelColor(i, isOn ? paint(i) : seg.color(1));
        }
        return seg.out();
    };

    return effect;
})();
