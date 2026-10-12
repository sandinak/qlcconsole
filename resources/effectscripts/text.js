/*
  QLC+ Effect Script: Text

  Words on a pixel grid -- a group's head layout (a run of LED strips, a
  matrix) -- in a 5 x 7 font: scrolling across from the right, or still in
  the middle. Drawn in the look's first colour, over its second when it has
  one. Not for WLED fixtures (they draw their own); for anything whose pixels
  are heads.
*/
(function() {
    var effect = new Object;
    effect.apiVersion  = 1;
    effect.name        = "Text";
    effect.description = "Words on a pixel grid: scrolling, or still in the middle";
    effect.author      = "qlcconsole";
    effect.fixtureTypes = ["rgb"];
    effect.notes = "Shows the text on the look's group as a grid of pixels (its head layout: a run of strips, a matrix), in a 5 x 7 font. Scroll brings it in from the right; Still holds it in the middle. The look's first colour draws the letters, its second (if any) fills behind them. Needs a grid at least 7 pixels tall for the whole letter; Size makes each dot bigger on a large matrix. If it reads backwards or upside down, use Mirror or Upside down.";

    effect.inputs = [];
    effect.palettes = [];
    effect.parameters = [
        { name: "text",     description: "The words to show", type: "text", defaultValue: "HELLO" },
        { name: "style",    description: "Scroll across, or still in the middle", type: "enum", values: ["Scroll", "Still"], defaultValue: 0 },
        { name: "speed",    description: "Scroll speed, pixels a second", min: 1.0, max: 60.0, defaultValue: 12.0 },
        { name: "size",     description: "Pixels for each dot of a letter", min: 1, max: 4, defaultValue: 1 },
        { name: "level",    description: "Brightness (0-1)", min: 0.0, max: 1.0, defaultValue: 1.0 },
        { name: "mirror",   description: "Mirror left to right (0/1)", min: 0, max: 1, defaultValue: 0 },
        { name: "flip",     description: "Upside down (0/1)", min: 0, max: 1, defaultValue: 0 }
    ];

    // 5 x 7, a column at a time, the lowest bit the top row: ASCII 32-126.
    var FONT = [
        "0000000000", "00005f0000", "0007000700", "147f147f14", "242a7f2a12", // space ! " # $
        "2313086462", "3649552250", "0000070000", "001c224100", "0041221c00", // % & ' ( )
        "142a7f2a14", "08083e0808", "0050300000", "0808080808", "0060600000", // * + , - .
        "2010080402", "3e5149453e", "00427f4000", "4261514946", "2141454b31", // / 0 1 2 3
        "1814127f10", "2745454539", "3c4a494930", "0171090503", "3649494936", // 4 5 6 7 8
        "064949291e", "0036360000", "0056360000", "0814224100", "1414141414", // 9 : ; < =
        "0041221408", "0201510906", "324979413e", "7e1111117e", "7f49494936", // > ? @ A B
        "3e41414122", "7f4141221c", "7f49494941", "7f09090901", "3e4149497a", // C D E F G
        "7f0808087f", "00417f4100", "2040413f01", "7f08142241", "7f40404040", // H I J K L
        "7f020c027f", "7f0408107f", "3e4141413e", "7f09090906", "3e4151215e", // M N O P Q
        "7f09192946", "4649494931", "01017f0101", "3f4040403f", "1f2040201f", // R S T U V
        "3f4038403f", "6314081463", "0708700807", "6151494543", "007f414100", // W X Y Z [
        "0204081020", "0041417f00", "0402010204", "4040404040", "0001020400", // \ ] ^ _ `
        "2054545478", "7f48444438", "3844444420", "384444487f", "3854545418", // a b c d e
        "087e090102", "0c5252523e", "7f08040478", "00447d4000", "2040443d00", // f g h i j
        "7f10284400", "00417f4000", "7c04180478", "7c08040478", "3844444438", // k l m n o
        "7c14141408", "081414187c", "7c08040408", "4854545420", "043f444020", // p q r s t
        "3c4040207c", "1c2040201c", "3c4030403c", "4428102844", "0c5050503c", // u v w x y
        "4464544c44", "0008364100", "00007f0000", "0041360800", "0804080408"  // z { | } ~
    ];
    var GLYPH_W = 5, GLYPH_H = 7, ADVANCE = 6;      // a column of space after each

    // Is dot (x, y) of the text lit? x across the whole text, y down a letter.
    function dot(text, x, y) {
        if (x < 0 || y < 0 || y >= GLYPH_H) return false;
        var ch = Math.floor(x / ADVANCE), gx = x - ch * ADVANCE;
        if (ch >= text.length || gx >= GLYPH_W) return false;
        var code = text.charCodeAt(ch);
        if (code < 32 || code > 126) code = 63;                 // ?
        var col = parseInt(FONT[code - 32].substr(gx * 2, 2), 16);
        return ((col >> y) & 1) === 1;
    }

    function scaled(c, level) {
        return { r: Math.round(c.r * level), g: Math.round(c.g * level), b: Math.round(c.b * level) };
    }

    effect.tick = function(fixtures, inputs, palettes, params, state) {
        var n = fixtures.length;
        if (n === 0) return [];
        var t      = inputs._time !== undefined ? inputs._time : 0;
        var text   = (params.text !== undefined && params.text !== null) ? String(params.text) : "HELLO";
        var still  = Math.round(params.style !== undefined ? params.style : 0) === 1;
        var speed  = params.speed !== undefined ? params.speed : 12.0;
        var size   = Math.max(1, Math.round(params.size !== undefined ? params.size : 1));
        var level  = params.level !== undefined ? params.level : 1.0;
        var mirror = (params.mirror || 0) >= 0.5;
        var flip   = (params.flip || 0) >= 0.5;

        var L  = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
        var fg = scaled((L[0] && L[0].r !== undefined) ? L[0] : { r: 255, g: 255, b: 255 }, level);
        var bg = (L[1] && L[1].r !== undefined) ? scaled(L[1], level) : { r: 0, g: 0, b: 0 };

        var g0 = fixtures[0].grid || { cols: n, rows: 1 };
        var cols = g0.cols || n, rows = g0.rows || 1;

        // Where the text's first dot is: still in the middle, or scrolling in
        // from the right edge and out past the left before coming round again.
        var width = Math.max(0, text.length * ADVANCE - 1) * size;
        var x0;
        if (still)
            x0 = Math.floor((cols - width) / 2);
        else
            x0 = cols - Math.floor(t * speed) % (width + cols);
        var y0 = Math.floor((rows - GLYPH_H * size) / 2);

        return fixtures.map(function(f, i) {
            var g = f.grid || { col: i, row: 0 };
            var col = mirror ? cols - 1 - g.col : g.col;
            var row = flip ? rows - 1 - g.row : g.row;
            var x = col - x0, y = row - y0;
            if (x >= 0 && y >= 0 && dot(text, Math.floor(x / size), Math.floor(y / size)))
                return fg;
            return bg;
        });
    };

    return effect;
})()
