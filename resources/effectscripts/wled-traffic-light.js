/*
  QLC+ Effect Script: WLED Traffic Light
  The strip runs a traffic light: green, amber, red and round; speed is how long each
  light stays.
  Written from what WLED's "Traffic Light" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Traffic Light";
    effect.description = "From WLED: green, amber, red";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Traffic Light";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var unit = WLED.period(seg.speed, 6000, 400);
        var t = seg.now % (5 * unit);
        var c = t < 2 * unit ? 0x00C000 : t < 2.6 * unit ? 0xFFA000 : 0xFF0000;
        seg.fill(c);
        return seg.out();
    };

    return effect;
})();
