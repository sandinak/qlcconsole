/*
  QLC+ Effect Script: WLED Lake
  Calm water: the colour (or palette) laid along the strip with slow, soft swells of
  brightness moving across it.
  Written from what WLED's "Lake" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Lake";
    effect.description = "From WLED: slow soft swells on still colour";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Lake";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var s = seg.now / WLED.period(seg.speed, 30000, 3000);
        for (var i = 0; i < len; i++) {
            var x = i / len;
            var swell = 0.5 + 0.25 * Math.sin(2 * Math.PI * (x * 1.5 + s)) + 0.25 * Math.sin(2 * Math.PI * (x * 3.3 - s * 1.6));
            var c = seg.palette ? seg.colorFromPalette(Math.floor((x * 128 + s * 40) % 256), false, true, 0) : seg.color(0);
            seg.setPixelColor(i, WLED.colorBlend(0, c, Math.round((0.25 + 0.75 * swell) * 255)));
        }
        return seg.out();
    };

    return effect;
})();
