/*
  QLC+ Effect Script: WLED TV Simulator
  Someone watching television in a dark room: the whole strip shifting between
  bluish and warm tints at random, brighter and dimmer, with sudden cuts.
  Written from what WLED's "TV Simulator" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED TV Simulator";
    effect.description = "From WLED: the flicker of a television in a dark room";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "TV Simulator";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var scene = WLED.period(seg.speed, 6000, 600);
        var n = Math.floor(seg.now / scene), f = (seg.now % scene) / scene;
        var tint = function(k) {
            var warm = WLED.hash(k, 100);
            return WLED.rgb(Math.round(80 + 140 * warm), Math.round(90 + 90 * WLED.hash(k, 101)), Math.round(210 - 120 * warm));
        };
        var c = WLED.colorBlend(tint(n), tint(n + 1), Math.round(Math.max(0, f - 0.85) / 0.15 * 255));
        var flick = 0.45 + 0.55 * WLED.noise(seg.now / 90, 102);
        seg.fill(WLED.colorBlend(0, c, Math.round(flick * (0.4 + seg.intensity / 255 * 0.6) * 255)));
        return seg.out();
    };

    return effect;
})();
