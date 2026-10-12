/*
  QLC+ Effect Script: WLED Sinelon Rainbow
  Like Sinelon, the dot changing hue as it swings.
  Written from what WLED's "Sinelon Rainbow" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sinelon Rainbow";
    effect.description = "From WLED: a swinging dot changing hue";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sinelon Rainbow";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 500);
        var fadeMs = 100 + (255 - seg.intensity) * 3;
        seg.fill(seg.color(1));
        for (var age = Math.round(fadeMs); age >= 0; age -= 16) {
            var t = seg.now - age;
            var a = Math.round((0.5 + 0.5 * Math.sin(t / period * 2 * Math.PI)) * (len - 1));
            seg.setPixelColor(a, WLED.colorBlend(seg.color(1), WLED.hue(Math.floor(t / 20) & 255), Math.round((1 - age / fadeMs) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
