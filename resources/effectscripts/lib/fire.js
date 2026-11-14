/*
  QLC+ Effect Script library: Fire

  The flame Firelight burns with, for every effect that burns: a flicker
  from a sum of incommensurable sines (no state, no Math.random: the same
  time and seed give the same flame), and the fire palette that flicker
  is coloured by -- deep red when it is low, orange in the middle,
  yellow-white at its brightest -- with Firelight's Coolness and
  Intensity.

  An effect says
      effect.requires = ["fire"];
  and, for each head,
      var flame = FIRE.flame(t, seed, speed);         // 0..1
      var c = FIRE.colour(flame, coolness, intensity); // {r, g, b}
*/
var FIRE = (function() {
    var F = {};

    /** The raw flicker: about 0..1, a little either side. */
    F.flicker = function(t, seed, speed) {
        var s = speed;
        return 0.5 + 0.25 * Math.sin(t * s * 1.0 + seed * 7.3)
                   + 0.15 * Math.sin(t * s * 2.3 + seed * 3.1)
                   + 0.10 * Math.sin(t * s * 5.7 + seed * 11.9);
    };

    /** The flicker, held to 0..1. */
    F.flame = function(t, seed, speed) {
        return Math.max(0, Math.min(1, F.flicker(t, seed, speed)));
    };

    /** Many flames at once, for an effect over thousands of heads: the
     *  same flicker as F.flame(t, seeds[i], speed), to the last bit or two,
     *  with each seed's three phases worked out once (F.phases, when the
     *  seeds change) and each tick's three sines once for every head
     *  (F.clock): sin(a + b) = sin a cos b + cos a sin b.
     *      var ph = F.phases(seeds);           // once
     *      var k = F.clock(t, speed);          // each tick
     *      var flame = F.flameAt(ph, i, k);    // each head */
    F.phases = function(seeds) {
        var n = seeds.length, ph = new Float64Array(n * 6);
        for (var i = 0; i < n; i++) {
            var sd = seeds[i], j = i * 6;
            ph[j]     = Math.sin(sd * 7.3);  ph[j + 1] = Math.cos(sd * 7.3);
            ph[j + 2] = Math.sin(sd * 3.1);  ph[j + 3] = Math.cos(sd * 3.1);
            ph[j + 4] = Math.sin(sd * 11.9); ph[j + 5] = Math.cos(sd * 11.9);
        }
        return ph;
    };

    F.clock = function(t, speed) {
        var a = t * speed * 1.0, b = t * speed * 2.3, c = t * speed * 5.7;
        return [Math.sin(a), Math.cos(a), Math.sin(b), Math.cos(b), Math.sin(c), Math.cos(c)];
    };

    F.flameAt = function(ph, i, k) {
        var j = i * 6;
        var v = 0.5 + 0.25 * (k[0] * ph[j + 1] + k[1] * ph[j])
                    + 0.15 * (k[2] * ph[j + 3] + k[3] * ph[j + 2])
                    + 0.10 * (k[4] * ph[j + 5] + k[5] * ph[j + 4]);
        return v < 0 ? 0 : (v > 1 ? 1 : v);
    };

    /** The fire colour of a flame 0..1: (64,0,0) at 0, (255,60,0) at 0.5,
     *  (255,200,20) at 1; @p cool shifts it toward blue-white (0 fire,
     *  1 ice), then @p inten scales it. Whole numbers, as DMX takes them. */
    F.colour = function(flame, cool, inten) {
        var r, g, b, f01;
        if (flame < 0.5) {
            f01 = flame * 2;
            r = Math.round(64  + f01 * (255 - 64));
            g = Math.round(0   + f01 * 60);
            b = 0;
        } else {
            f01 = (flame - 0.5) * 2;
            r = 255;
            g = Math.round(60  + f01 * (200 - 60));
            b = Math.round(0   + f01 * 20);
        }

        r = Math.round(r * (1 - cool) + 200 * cool);
        g = Math.round(g * (1 - cool) + 220 * cool);
        b = Math.round(b * (1 - cool) + 255 * cool);

        return { r: Math.round(r * inten),
                 g: Math.round(g * inten),
                 b: Math.round(b * inten) };
    };

    return F;
})();
