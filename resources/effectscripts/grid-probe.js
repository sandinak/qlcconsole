/*
  QLC+ Effect Script: Grid Probe (diagnostic)
  Paints each fixture by its GRID CELL so you can see how the effect engine maps
  your rig: RED brightens with COLUMN (left→right), BLUE brightens with ROW
  (top→bottom), and the col-0 / row-0 edges are tinted GREEN to mark the origin.

  Use it to check a pixel/MIDI grid: on a correctly laid-out wide panel you
  should see red get brighter LEFT→RIGHT and blue brighter TOP→BOTTOM. If red
  brightens top→bottom instead, the head layout is TRANSPOSED (physical columns
  stored as layout rows) — recreate the group with "Create group from selection"
  after placing the fixtures in the 2-D monitor.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Grid Probe";
    effect.description = "Diagnostic: colours fixtures by grid column/row";
    effect.author      = "QLC+";
    effect.fixtureTypes = ["rgb", "dimmer"];
    effect.notes = "Diagnostic tool. RED = column (left→right), BLUE = row (top→bottom), GREEN edge = col 0 / row 0. Red should brighten across the physical WIDTH; if it brightens down the height, your head layout is transposed. The tooltip target size prints as cols×rows via the first cell.";

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n = fixtures.length;
        if (n === 0) return [];
        var g0 = fixtures[0].grid || { cols: n, rows: 1 };
        var cols = g0.cols || 1, rows = g0.rows || 1;

        return fixtures.map(function(f) {
            var g = f.grid || { col: 0, row: 0 };
            var cx = cols > 1 ? g.col / (cols - 1) : 0;   // 0..1 across columns
            var ry = rows > 1 ? g.row / (rows - 1) : 0;   // 0..1 down rows
            var edge = (g.col === 0 || g.row === 0) ? 140 : 0;
            var out = {
                r: Math.round(cx * 255),
                g: edge,
                b: Math.round(ry * 255)
            };
            if (f.hasDimmer) out.dimmer = 1.0;
            return out;
        });
    };

    return effect;
})()
