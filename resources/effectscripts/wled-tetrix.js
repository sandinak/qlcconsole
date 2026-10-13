/*
  QLC+ Effect Script: WLED Tetrix
  Blocks of colour drop one after another from the far end and stack up at the start,
  until the strip is full; then it clears and starts again.
  Written from what WLED's "Tetrix" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Tetrix";
    effect.description = "From WLED: blocks dropping and stacking up";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Tetrix";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var drop = WLED.period(seg.speed, 2500, 250);           // one block's fall
        var size = Math.max(1, Math.round(1 + seg.intensity / 255 * 7));
        var blocks = Math.ceil(len / size);
        var cycle = blocks * drop + drop;
        var n = Math.floor(seg.now / cycle), t = seg.now % cycle;
        var landed = Math.min(blocks, Math.floor(t / drop));
        seg.fill(seg.color(1));
        var colOf = function(b) { return seg.palette ? seg.colorFromPalette(Math.floor(WLED.hash(n, b) * 255), false, true, 0)
                                                    : WLED.hue(Math.floor(WLED.hash(n, b) * 256)); };
        for (var b = 0; b < landed; b++)
            for (var k = 0; k < size; k++) seg.setPixelColor(b * size + k, colOf(b));
        if (landed < blocks) {
            var f = (t % drop) / drop;
            var top = len, floor = landed * size;
            var at = Math.round(top - (top - floor) * f * f);
            for (var k = 0; k < size; k++) seg.setPixelColor(at + k, colOf(landed));
        }
        return seg.out();
    };

    return effect;
})();
