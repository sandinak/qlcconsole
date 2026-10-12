/*
  QLC+ Effect Script: WLED device effect (stand-in)
  For a look running a WLED effect that has no port yet: WLED fixtures are
  sent the effect and run it themselves (QLCPalette::wledEffect()); this only
  gives the look WLED's own controls. Every other fixture in the look is left
  as the rest of the look sets it. Never listed in the effect browser.
*/
(function() {
    var effect = new Object;
    effect.name        = "WLED device effect";
    effect.description = "Runs on WLED fixtures themselves; nothing on others";
    effect.apiVersion  = 1;
    effect.version     = 1;
    effect.hidden      = true;
    effect.requires    = ["wled"];
    effect.fixtureTypes = ["rgb"];
    effect.parameters  = WLED.parameters();

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        return [];
    };

    return effect;
})();
