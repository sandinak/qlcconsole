/*
  QLC+ Effect Script: WLED Meteor
  A meteor: a bright head shooting along the strip, its tail breaking up and fading
  behind it; intensity is the length of the tail.
  Written from what WLED's "Meteor" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Meteor";
    effect.description = "From WLED: a meteor with a breaking-up tail";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Meteor";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var period = WLED.period(seg.speed, 6000, 300);
        var travel = len * 1.6;                                 // runs off the end before coming back
        var head = (seg.now % period) / period * travel;
        var tail = 2 + seg.intensity / 255 * len / 2;
        var flick = Math.floor(seg.now / 60);
        for (var i = 0; i < len; i++) {
            var behind = head - i;
            var lvl = 0;
            if (behind >= 0 && behind < 1) lvl = 1;
            else if (behind >= 1 && behind < tail)
                lvl = (1 - behind / tail) * (0.4 + 0.6 * WLED.hash(i, flick));
            seg.setPixelColor(i, WLED.colorBlend(seg.color(1), paint(i), Math.round(lvl * 255)));
        }
        return seg.out();
    };

    return effect;
})();
