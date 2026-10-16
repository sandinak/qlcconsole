/*
  QLC+ Effect Script: WLED Shimmer
  A shimmer of light sweeping across the strip over its colours, then a pause
  before the next; speed is how fast it crosses, intensity how long the pause.
  Written from what WLED's "Shimmer" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Shimmer";
    effect.description = "From WLED: a shimmer sweeping across the strip";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Shimmer";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var move = WLED.period(seg.speed, 4000, 400);
        var cycle = move + seg.intensity / 255 * 4000;
        var t = seg.now % cycle;
        var size = Math.max(2, len / 4);
        var pos = t < move ? -size + t / move * (len + 2 * size) : -1e9;
        for (var i = 0; i < len; i++) {
            var d = Math.abs(i - pos) / size;
            var boost = d < 1 ? 0.5 + 0.5 * Math.cos(d * Math.PI) : 0;
            var c = paint(i);
            seg.setPixelColor(i, WLED.mix(WLED.dim(c, 0.35), WLED.colorBlend(c, 0xFFFFFF, 90), boost));
        }
        return seg.out();
    };

    return effect;
})();
