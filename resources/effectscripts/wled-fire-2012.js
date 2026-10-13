/*
  QLC+ Effect Script: WLED Fire 2012
  Flames: hot at the base of the strip, flickering and cooling as they rise; speed is how
  lively, intensity how high the flames reach. Colours run black, red, yellow, white
  (or the palette, when one is set).
  Written from what WLED's "Fire 2012" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Fire 2012";
    effect.description = "From WLED: flickering flames rising from the base";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Fire 2012";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };

        var rate = WLED.period(255 - seg.speed, 120, 600);      // ms per flicker
        var reach = 0.25 + seg.intensity / 255 * 0.75;
        var t = seg.now / rate;
        for (var i = 0; i < len; i++) {
            var h = i / Math.max(1, len - 1);                   // 0 at the base
            var flame = reach * (0.55 + 0.45 * WLED.noise(t * 0.7, 3)) + 0.15 * (WLED.noise(t + i * 0.9, 7) - 0.5);
            var heat = Math.max(0, Math.min(1, 1.15 - h / Math.max(0.05, flame)))
                       * (0.75 + 0.25 * WLED.noise(t * 1.7 + i * 1.3, 11));
            var c;
            if (seg.palette) c = seg.colorFromPalette(Math.round(heat * 240), false, false, 0);
            else if (heat < 0.33) c = WLED.rgb(Math.round(heat * 3 * 255), 0, 0);
            else if (heat < 0.66) c = WLED.rgb(255, Math.round((heat - 0.33) * 3 * 255), 0);
            else c = WLED.rgb(255, 255, Math.round((heat - 0.66) * 3 * 255));
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
