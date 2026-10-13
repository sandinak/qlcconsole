/*
  QLC+ Effect Script: WLED Colorwaves
  Like Pride, but in the palette's colours (or colours 1-3): waves of colour and
  brightness moving through each other.
  Written from what WLED's "Colorwaves" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Colorwaves";
    effect.description = "From WLED: waves of the palette's colour and brightness";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Colorwaves";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 40000, 3000);
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var idx = Math.floor((s * 200 + x * 256 * (1 + 0.5 * Math.sin(s * 2.1))) % 256);
            var c = seg.colorFromPalette(idx, false, true, 0);
            if (!seg.palette)
                c = WLED.colorBlend(seg.color(0), seg.color(2) || seg.color(1) || 0x0000FF,
                                    Math.round((0.5 - 0.5 * Math.cos(idx / 256 * 2 * Math.PI)) * 255));
            var bri = 0.5 + 0.5 * Math.sin(2 * Math.PI * (x * 2 - s * 1.7));
            seg.setPixelColor(i, WLED.colorBlend(0, c, Math.round((0.3 + 0.7 * bri) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
