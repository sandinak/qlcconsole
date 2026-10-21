/*
  QLC+ Effect Script: WLED PS Hourglass
  An hourglass: grains drop one at a time from the pile at the top, fall and stack
  up at the bottom; when the last has fallen the glass turns over and it starts
  again. Speed is how often a grain drops, intensity how many grains.
  Written from what WLED's "PS Hourglass" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS Hourglass";
    effect.description = "From WLED: sand falling through an hourglass";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS Hourglass";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var grains = Math.max(1, Math.round(len * (0.2 + 0.6 * seg.intensity / 255)));
        var interval = WLED.period(seg.speed, 3000, 100);
        var g = len * 4;
        var cycle = grains * interval + Math.sqrt(2 * len / g) * 1000 + 1000;
        var flip = Math.floor(seg.now / cycle) % 2, t = seg.now % cycle;
        seg.fill(seg.color(1));
        for (var k = 0; k < grains; k++) {
            var from = len - grains + k;                        // its place in the pile at the top
            var y = from;
            var tf = (t - k * interval) / 1000;
            if (tf > 0) y = Math.max(k, from - g * tf * tf / 2);    // falling, then resting on the pile below
            var p = Math.round(y);
            if (flip) p = len - 1 - p;
            seg.setPixelColor(p, seg.palette ? seg.colorFromPalette(Math.floor(k * 255 / grains), false, false, 0)
                                             : seg.color(0));
        }
        return seg.out();
    };

    return effect;
})();
