/*
  QLC+ Effect Script: WLED PS DripDrop
  Drops swelling at the top of the strip, falling faster and faster and splashing
  at the bottom in little droplets; speed is how fast they fall, intensity how
  often.
  Written from what WLED's "PS DripDrop" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PS DripDrop";
    effect.description = "From WLED: dripping drops that splash";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PS DripDrop";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var every = WLED.period(seg.intensity, 3000, 300);      // a drop every so often
        var g = len * (0.6 + seg.speed / 255 * 2.4);            // pixels/s/s
        var top = len - 1;
        var grow = every * 0.5;
        var fall = Math.sqrt(2 * top / g);                      // seconds to the bottom
        var splash = 0.8;
        seg.fill(seg.color(1));
        var cur = Math.floor(seg.now / every);
        for (var s = cur - Math.ceil((grow + (fall + splash) * 1000) / every); s <= cur; s++) {
            var t = seg.now - s * every;
            if (t < 0) continue;
            var c = seg.palette ? seg.randomColor(WLED.hash(s, 230)) : seg.color(0);
            if (t < grow) { seg.setPixelColor(top, WLED.mix(seg.getPixelColor(top), c, t / grow)); continue; }
            var tf = (t - grow) / 1000;
            if (tf < fall) {
                var y = top - g * tf * tf / 2, k = Math.floor(y), w = y - k;
                seg.setPixelColor(k, WLED.add(seg.getPixelColor(k), WLED.dim(c, 1 - w)));
                seg.setPixelColor(k + 1, WLED.add(seg.getPixelColor(k + 1), WLED.dim(c, w)));
                continue;
            }
            var ts = tf - fall;
            if (ts >= splash) continue;
            for (var d = 0; d < 4; d++) {
                var v = (0.1 + 0.25 * WLED.hash(s, 231 + d)) * g * 0.6;
                var h = v * ts - g * 0.6 * ts * ts / 2;
                if (h < 0) continue;
                var p = Math.round(h);
                seg.setPixelColor(p, WLED.add(seg.getPixelColor(p), WLED.dim(c, (1 - ts / splash) * 0.8)));
            }
        }
        return seg.out();
    };

    return effect;
})();
