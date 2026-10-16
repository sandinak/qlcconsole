/*
  QLC+ Effect Script: WLED Ripple Peak
  Each beat of the music starts a ripple at a random place, spreading both ways
  and fading; speed is how quickly they fade, intensity how many at once.
  Written from what WLED's "Ripple Peak" does on a device and its description --
  not from WLED's code. Runs on WLED fixtures as WLED's own effect, and on
  any other RGB fixtures as this.
  Reacts to the audio input (Inputs/Outputs) as WLED reacts to its
  microphone; with no audio it rests.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED Ripple Peak";
    effect.description = "From WLED: ripples started by the beat";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.wled        = "Ripple Peak";
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.dataChannels = ["audio"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state, data) {
        var seg = WLED.segment(fixtures, inputs, palettes, params);
        var len = seg.len;
        if (len < 1) return seg.out();
        var paint = function(i) { return seg.colorFromPalette(i, true, false, 0); };
        var au = WLED.audio(data);
        WLED.recall(seg, state);
        seg.fill(0);
        var rip = state.ripples || [];
        var maxR = 1 + Math.floor(seg.intensity / 255 * 7);
        if (WLED.beat(state, au, seg.now) && rip.length < maxR)
            rip.push({ pos: Math.floor(WLED.hash(seg.now, 370) * len), born: seg.now, h: WLED.hash(seg.now, 371) });
        var life = 0.6 + (255 - seg.speed) / 255 * 1.4;
        var live = [];
        for (var k = 0; k < rip.length; k++) {
            var age = (seg.now - rip[k].born) / 1000;
            if (age < 0 || age >= life) continue;
            live.push(rip[k]);
            var r = Math.round(age * len * 0.5), c = WLED.dim(seg.randomColor(rip[k].h), 1 - age / life);
            seg.setPixelColor(rip[k].pos - r, WLED.add(seg.getPixelColor(rip[k].pos - r), c));
            if (r) seg.setPixelColor(rip[k].pos + r, WLED.add(seg.getPixelColor(rip[k].pos + r), c));
        }
        state.ripples = live;
        WLED.keep(seg, state);
        return seg.out();
    };

    return effect;
})();
