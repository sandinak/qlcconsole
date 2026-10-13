/*
  QLC+ Effect Script: WLED Colorloop
  The whole strip goes slowly round the colour wheel together; intensity
  washes the colours toward white below half way (full colour from 128 up).
  Written from what WLED's "Colorloop" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Colorloop";
    effect.description = "From WLED: the whole strip loops round the colour wheel";
    effect.apiVersion  = 1;
    effect.version     = 2;
    effect.wled        = "Colorloop";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 30000, 1500);
        var h = Math.floor((seg.now % period) / period * 256);
        seg.fill(WLED.hue(h, Math.min(255, seg.intensity * 2)));
        return seg.out();
    };

    return effect;
})();
