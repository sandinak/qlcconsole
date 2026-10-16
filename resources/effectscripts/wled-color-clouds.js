/*
  QLC+ Effect Script: WLED Color Clouds
  Soft, slow clouds of colour drifting along the strip and slowly changing hue;
  speed is how they drift, intensity how fast the colours change.
  Written from what WLED's "Color Clouds" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Color Clouds";
    effect.description = "From WLED: soft, slowly evolving clouds of colour";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Color Clouds";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 60000, 4000);
        var cs = seg.now / WLED.period(seg.intensity, 60000, 4000);
        for (var i = 0; i < len; i++) {
            var cloud = WLED.noise(i * 0.05 + s, 180);
            var h = WLED.noise(i * 0.03 - s * 0.5 + 7, 181) * 0.5 + cs;
            seg.setPixelColor(i, WLED.dim(seg.randomColor(h - Math.floor(h)), 0.2 + 0.8 * cloud * cloud));
        }
        return seg.out();
    };

    return effect;
})();
