/*
  QLC+ Effect Script: WLED Sinelon Dual
  Like Sinelon, with a second dot of colour 3 swinging the opposite way.
  Written from what WLED's "Sinelon Dual" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Sinelon Dual";
    effect.description = "From WLED: two dots swinging opposite ways with trails";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Sinelon Dual";
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
        var other = seg.color(2) || seg.color(0);
        for (var age = Math.round(fadeMs); age >= 0; age -= 16) {
            var ph = (seg.now - age) / period * 2 * Math.PI;
            var a = Math.round((0.5 + 0.5 * Math.sin(ph)) * (len - 1));
            var b = len - 1 - a;
            var lvl = Math.round((1 - age / fadeMs) * 255);
            seg.setPixelColor(a, WLED.colorBlend(seg.color(1), paint(a), lvl));
            seg.setPixelColor(b, WLED.colorBlend(seg.color(1), other, lvl));
        }
        return seg.out();
    };

    return effect;
})();
