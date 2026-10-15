/*
  QLC+ Effect Script: WLED Lighthouse
  A dot running from the start to the end of the strip, over and over, a fading
  trail behind it; intensity is how fast the trail fades.
  Written from what WLED's "Lighthouse" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Lighthouse";
    effect.description = "From WLED: a dot with a fading trail, start to end";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Lighthouse";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 6000, 300);
        var head = (seg.now % period) / period * len;
        var tail = 1 + (255 - seg.intensity) / 255 * len * 0.8;
        for (var i = 0; i < len; i++) {
            var behind = ((head - i) % len + len) % len;
            var lvl = behind < 1 ? 1 : Math.max(0, 1 - behind / tail);
            seg.setPixelColor(i, WLED.mix(seg.color(1), paint(i), lvl));
        }
        return seg.out();
    };

    return effect;
})();
