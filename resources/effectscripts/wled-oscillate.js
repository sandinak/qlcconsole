/*
  QLC+ Effect Script: WLED Oscillate
  Three wide bands of colours 1, 2 and 3 sliding back and forth through each other,
  where they overlap the colours add.
  Written from what WLED's "Oscillate" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Oscillate";
    effect.description = "From WLED: three bands sliding back and forth";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Oscillate";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var base = WLED.period(seg.speed, 6000, 600);
        var w = Math.max(2, len * (0.2 + seg.intensity / 255 * 0.3));
        var cols = [seg.color(0) || 0xFF0000, seg.color(1) || 0x00FF00, seg.color(2) || 0x0000FF];
        for (var i = 0; i < len; i++) {
            var r = 0, g = 0, b = 0;
            for (var k = 0; k < 3; k++) {
                var c = (len - w) * (0.5 + 0.5 * Math.sin(seg.now / (base * (1 + k * 0.37)) * 2 * Math.PI + k * 2));
                if (i >= c && i < c + w) { r += (cols[k] >> 16) & 255; g += (cols[k] >> 8) & 255; b += cols[k] & 255; }
            }
            seg.setPixelColor(i, WLED.rgb(Math.min(255, r), Math.min(255, g), Math.min(255, b)));
        }
        return seg.out();
    };

    return effect;
})();
