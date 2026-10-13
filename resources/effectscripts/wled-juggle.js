/*
  QLC+ Effect Script: WLED Juggle
  Eight coloured dots swing back and forth at different rates, crossing and leaving
  short trails -- juggling balls.
  Written from what WLED's "Juggle" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Juggle";
    effect.description = "From WLED: eight coloured dots juggling";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Juggle";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var base = WLED.period(seg.speed, 12000, 800);
        seg.fill(seg.color(1));
        for (var d = 0; d < 8; d++) {
            var period = base * (1 + d * 0.13);
            var hueD = (d * 32) & 0xFF;
            for (var k = 0; k < 6; k++) {                      // the dot and its trail
                var t = seg.now - k * period / 60;
                var p = Math.round((0.5 - 0.5 * Math.cos(((t % period) + period) % period / period * 2 * Math.PI)) * (len - 1));
                var a = Math.round((1 - k / 6) * (0.3 + seg.intensity / 255 * 0.7) * 255);
                seg.setPixelColor(p, WLED.colorBlend(seg.getPixelColor(p), WLED.hue(hueD), a));
            }
        }
        return seg.out();
    };

    return effect;
})();
