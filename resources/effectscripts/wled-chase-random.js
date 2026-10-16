/*
  QLC+ Effect Script: WLED Chase Random
  Chase, leaving a trail of random colour: a block runs round the strip and every
  pixel it passes takes on that lap's new colour; intensity is the block's width.
  Written from what WLED's "Chase Random" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase Random";
    effect.description = "From WLED: a chase leaving a random colour behind it";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase Random";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 300);         // one lap
        var size = Math.max(1, Math.round(1 + seg.intensity / 255 * len / 6));
        var lap = Math.floor(seg.now / period);
        var head = (seg.now % period) / period * len;
        for (var i = 0; i < len; i++) {
            var behind = head - i;
            if (behind >= 0 && behind < size) seg.setPixelColor(i, paint(i));
            else seg.setPixelColor(i, seg.randomColor(WLED.hash(i < head ? lap : lap - 1, 33)));
        }
        return seg.out();
    };

    return effect;
})();
