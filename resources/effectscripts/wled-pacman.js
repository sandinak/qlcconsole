/*
  QLC+ Effect Script: WLED PacMan
  Pac-Man along the strip: he runs eating a row of dots, the ghosts behind him; a
  power dot turns them blue and they flee, blinking as it wears off. Intensity is
  how many power dots.
  Written from what WLED's "PacMan" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED PacMan";
    effect.description = "From WLED: Pac-Man eating dots, chased by ghosts";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "PacMan";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var lap = WLED.period(seg.speed, 20000, 3000);
        var span = len + 12;
        var t = (seg.now % lap) / lap;
        var pac = t * span - 2;
        var gap = Math.round(30 - seg.intensity / 255 * 24);    // a power dot every so many pixels
        var isPower = function(i) { return i > 2 && (i % gap) === gap - 1; };
        seg.fill(0);
        for (var i = 0; i < len; i++) {
            if (i <= pac) continue;                             // eaten
            if (isPower(i)) seg.setPixelColor(i, (Math.floor(seg.now / 125) % 2) ? 0xFFFFFF : 0x606060);
            else if ((i % 2) === 0) seg.setPixelColor(i, 0x303030);
        }
        // The last power dot he ate, and how long ago.
        var since = -1, eatenAt = 0;
        for (var p = Math.min(len - 1, Math.floor(pac)); p >= 0; p--)
            if (isPower(p)) { eatenAt = p; since = (pac - p) / span * lap; break; }
        var fright = 2500;
        var cols = [0xFF0000, 0xFFB8FF, 0x00FFFF, 0xFFB852];
        for (var g = 0; g < 4; g++) {
            var gp, gc = cols[g];
            if (since >= 0 && since < fright) {
                gp = eatenAt - 3 - 2 * g - since / 1000 * 4;    // fleeing the other way
                var ending = since > fright * 0.7;
                gc = (ending && (Math.floor(seg.now / 150) % 2)) ? 0xFFFFFF : 0x0000FF;
            } else {
                gp = pac - 3 - 2 * g;
            }
            seg.setPixelColor(Math.round(gp), gc);
        }
        seg.setPixelColor(Math.round(pac), (Math.floor(seg.now / 90) % 2) ? 0xFFFF00 : 0xA0A000);
        return seg.out();
    };

    return effect;
})();
