/*
  QLC+ Effect Script: WLED Heartbeat
  A heartbeat: a strong beat and a softer one close behind, then a rest, in colour 1;
  speed is the heart rate.
  Written from what WLED's "Heartbeat" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Heartbeat";
    effect.description = "From WLED: lub-dub, lub-dub";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Heartbeat";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 2000, 400);         // 30 .. 150 a minute
        var t = (seg.now % period) / period;
        var beat = function(at, size) {
            var d = t - at;
            return d < 0 ? 0 : size * Math.exp(-d * 18);
        };
        var lvl = Math.min(1, beat(0, 1) + beat(0.22, 0.6));
        var a = Math.round(lvl * 255);
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), a));
        return seg.out();
    };

    return effect;
})();
