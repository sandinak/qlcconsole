/*
  QLC+ Effect Script: WLED Pride 2015
  An ever-changing rainbow: the hues drift and stretch along the strip while waves of
  brightness roll through them.
  Written from what WLED's "Pride 2015" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Pride 2015";
    effect.description = "From WLED: a shifting rainbow with rolling brightness";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Pride 2015";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 40000, 3000);
        var stretch = 0.6 + 0.4 * Math.sin(s * 2 * Math.PI * 0.37);
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var hue = Math.floor((s * 256 + x * 256 * stretch * 2) % 256);
            var bri = 0.55 + 0.45 * Math.sin(2 * Math.PI * (x * (1.5 + stretch) - s * 1.3));
            seg.setPixelColor(i, WLED.colorBlend(0, WLED.hue(hue), Math.round(bri * 255)));
        }
        return seg.out();
    };

    return effect;
})();
