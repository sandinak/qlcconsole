/*
  QLC+ Effect Script: WLED Strobe Mega
  Colour 2 throughout, and every so often a burst of several short flashes of the
  palette (or colour 1); intensity is how many flashes a burst has.
  Written from what WLED's "Strobe Mega" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Strobe Mega";
    effect.description = "From WLED: bursts of short flashes";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Strobe Mega";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var gap = WLED.period(seg.speed, 3000, 300);            // from one burst to the next
        var flashes = 1 + Math.floor(seg.intensity / 255 * 7);
        var step = Math.min(70, gap / (2 * flashes + 1));
        var k = Math.floor((seg.now % gap) / step);
        var on = k < 2 * flashes && (k % 2) === 0;
        for (var i = 0; i < len; i++)
            seg.setPixelColor(i, on ? paint(i) : seg.color(1));
        return seg.out();
    };

    return effect;
})();
