/*
  QLC+ Effect Script: WLED Railway
  A level-crossing light: alternate pixels swap between colour 1 and colour 2 with a
  soft cross-fade.
  Written from what WLED's "Railway" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Railway";
    effect.description = "From WLED: alternate pixels swapping, crossing light";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Railway";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 3000, 300);
        var t = (seg.now % period) / period;
        var f = Math.max(0, Math.min(1, (Math.abs(t * 2 - 1) - 0.5 + seg.intensity / 510) * 4 + 0.5));
        for (var i = 0; i < len; i++) {
            var a = (i % 2) ? f : 1 - f;
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(a * 255)));
        }
        return seg.out();
    };

    return effect;
})();
