/*
  QLC+ Effect Script: WLED Chase Flash Rnd
  Chase Flash in random colours: two pixels at a time flash in random colours and
  are left a random colour as the flashing moves on along the strip.
  Written from what WLED's "Chase Flash Rnd" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Chase Flash Rnd";
    effect.description = "From WLED: pairs of pixels flashing, left in random colours";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Chase Flash Rnd";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var period = WLED.period(seg.speed, 1500, 120);
        var n = Math.floor(seg.now / period), t = (seg.now % period) / period;
        var per = Math.ceil(len / 2);                           // steps in a lap
        var lap = Math.floor(n / per), at = n % per;
        var flash = (t < 0.15) || (t > 0.3 && t < 0.45);
        for (var i = 0; i < len; i++) {
            var blk = Math.floor(i / 2);
            var c;
            if (blk === at)
                c = flash ? seg.randomColor(WLED.hash(n, 35)) : seg.color(1);
            else
                c = seg.randomColor(WLED.hash((blk < at ? lap : lap - 1) * 131 + blk, 34));
            seg.setPixelColor(i, c);
        }
        return seg.out();
    };

    return effect;
})();
