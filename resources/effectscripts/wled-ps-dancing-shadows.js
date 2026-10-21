/*
  QLC+ Effect Script: WLED PS Dancing Shadows
  Soft shadows of colour rushing across the strip, some one way, some the other,
  at their own speeds, slowly changing hue; intensity is how many.
  Written from what WLED's "PS Dancing Shadows" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Dancing Shadows";
    effect.description = "From WLED: soft shadows rushing across";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Dancing Shadows";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var count = 2 + Math.floor(seg.intensity / 255 * 6);
        var cross = WLED.period(seg.speed, 6000, 600) / 1000;   // seconds to cross, at full speed
        seg.fill(0);
        for (var k = 0; k < count; k++) {
            var w = 2 + WLED.hash(k, 202) * len / 6;
            var span = len + 2 * w;
            var v = (0.3 + 0.7 * WLED.hash(k, 200)) * len / cross;
            var dir = WLED.hash(k, 201) < 0.5 ? 1 : -1;
            var x = WLED.hash(k, 203) * span + dir * v * seg.now / 1000;
            var pos = ((x % span) + span) % span - w;
            var h = WLED.hash(k, 204) + seg.now / 40000;
            var c = seg.randomColor(h - Math.floor(h));
            for (var i = Math.max(0, Math.floor(pos - w)); i <= Math.min(len - 1, Math.ceil(pos + w)); i++) {
                var d = Math.abs(i - pos) / w;
                if (d < 1) seg.setPixelColor(i, WLED.add(seg.getPixelColor(i), WLED.dim(c, 0.5 + 0.5 * Math.cos(d * Math.PI))));
            }
        }
        return seg.out();
    };

    return effect;
})();
