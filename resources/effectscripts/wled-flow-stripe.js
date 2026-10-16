/*
  QLC+ Effect Script: WLED Flow Stripe
  A strip of rotating colours: hues rippling along it and turning round the wheel
  (or the palette); speed is how fast the hues turn, intensity how fast the ripple
  runs.
  Written from what WLED's "Flow Stripe" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Flow Stripe";
    effect.description = "From WLED: a strip of rotating colours";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Flow Stripe";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var hs = seg.now / WLED.period(seg.speed, 20000, 1000);
        var es = seg.now / WLED.period(seg.intensity, 8000, 500);
        for (var i = 0; i < len; i++) {
            var h = i / Math.max(1, len) + 0.25 * Math.sin(i * 0.3 + es * 2 * Math.PI) + hs;
            seg.setPixelColor(i, seg.randomColor(h - Math.floor(h)));
        }
        return seg.out();
    };

    return effect;
})();
