/*
  QLC+ Effect Script: WLED Washing Machine
  The colours (or palette) laid along the strip swish one way, slow down, and swish
  back -- like a drum turning.
  Written from what WLED's "Washing Machine" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Washing Machine";
    effect.description = "From WLED: colours swishing one way and back";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Washing Machine";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 8000, 1200);
        var off = Math.sin(seg.now / period * 2 * Math.PI) * len * (0.3 + seg.intensity / 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, (seg.palette ? seg.colorFromPalette(Math.floor(((i + off) / len * 255) % 256 + 256) % 256, false, true, 0) : WLED.hue(Math.floor(((i + off) / len * 255) % 256 + 256) % 256)));
        return seg.out();
    };

    return effect;
})();
