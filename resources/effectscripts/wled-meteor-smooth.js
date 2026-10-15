/*
  QLC+ Effect Script: WLED Meteor Smooth
  Meteor with a smooth tail: a bright head gliding along the strip, the tail
  fading evenly behind it; intensity is the tail's length.
  Written from what WLED's "Meteor Smooth" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Meteor Smooth";
    effect.description = "From WLED: a meteor with a smooth tail";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Meteor Smooth";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 300);
        var head = (seg.now % period) / period * len * 1.6;     // runs off the end before coming back
        var tail = 2 + seg.intensity / 255 * len / 2;
        for (var i = 0; i < len; i++) {
            var behind = head - i;
            var lvl = behind < -1 ? 0 : behind < 0 ? 1 + behind : behind < tail ? 1 - behind / tail : 0;
            seg.setPixelColor(i, WLED.mix(seg.color(1), paint(i), lvl));
        }
        return seg.out();
    };

    return effect;
})();
