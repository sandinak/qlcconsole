/* Show Tree practice: a browser version of qlcconsole's Show Tree, for the
   Show Tree course (docs/course/). It is a practice copy, not the app: it
   covers Lesson 1.1 -- productions, cue lists, cues, layers with a colour, a
   level and an aim, tracking, and GO with fades -- on the teaching show's
   stage (show-data.js, made by docs/course/tools/export-web-show.py).

   Plain JavaScript, no build step, no network. Work is kept in this
   browser (localStorage) and can be saved to / opened from a file.
   Licence: Apache-2.0, as qlcconsole is. */
(function () {
  'use strict';

  var SHOW = window.SHOW_DATA;
  var root = document.getElementById('app');
  if (!SHOW || !root) {
    if (root) root.textContent = 'The show data (show-data.js) did not load.';
    return;
  }

  var STORE_KEY = 'qlcconsole-showtree-practice-v1';
  var FILE_KIND = 'qlcconsole Show Tree practice';

  /* ------------------------------------------------------------ the show -- */

  var FX = new Map();
  SHOW.fixtures.forEach(function (f) { FX.set(f.id, f); });
  var GROUPS = new Map();
  SHOW.groups.forEach(function (g) { GROUPS.set(g.name.toLowerCase(), g); });
  var PAL = { Colour: [], Dimmer: [], Position: [], Shutter: [] };
  var PALBY = new Map();
  SHOW.palettes.forEach(function (p) {
    (PAL[p.type] = PAL[p.type] || []).push(p);
    PALBY.set(p.name.toLowerCase(), p);
  });
  var TARGETS = new Map();
  SHOW.targets.forEach(function (t) { TARGETS.set(t.id, t); });
  var ATTR_OF = { Colour: 'colour', Dimmer: 'level', Position: 'aim', Shutter: 'shutter' };
  var CENTRE_X = (function () {
    var c = SHOW.stage.cyc;
    return c ? c.x : 8;
  })();

  var GROUP_ABOUT = {
    'steps': 'The LED strips on the front of all 12 riser steps (US, CS and DS rows). Colour only: a level dims the colour.',
    'us': 'The step fronts of the US row (the top row, upstage).',
    'cs': 'The step fronts of the CS (middle) row.',
    'ds': 'The step fronts of the DS row (the front row, downstage).',
    'steptops': 'The 24 bars on top of the riser steps, lighting up at the singers. Colour only.',
    'trusses': 'Everything on the four upright trusses.',
    'trusses sr': 'Everything on the two stage-right trusses.',
    'trusses sl': 'Everything on the two stage-left trusses.',
    'truss-spots': 'The moving spot on top of each truss, with a colour wheel and a dimmer.',
    'trussw-rgb': 'The colour (RGB) pixels of the truss boxes that wash out over the stage.',
    'trussw-w': 'The white pixels of the truss boxes that wash out over the stage.',
    'trussc-rgb': 'The colour (RGB) pixels of the truss boxes that light the cyc.',
    'trussc-w': 'The white pixels of the truss boxes that light the cyc.',
    'usts': 'The 8 upstage beams (two heads each) standing on the top riser. They have a dimmer.',
    'at-spots': 'The moving spots on the audience towers, for soloists. Dimmer and a shutter that rests closed.',
    'at-w-all': 'The 4 moving washes on the audience towers: front light. They have a dimmer.',
    'lanterns': 'The 6 LED lanterns hanging from the trusses and towers. They have a dimmer.'
  };

  function groupOf(name) { return GROUPS.get(String(name).toLowerCase()); }
  function palOf(name) { return name ? PALBY.get(String(name).toLowerCase()) : null; }

  /* ---------------------------------------------------------- the work -- */

  function emptyWork() { return { v: 1, productions: [], current: null, nextId: 1 }; }

  var work = emptyWork();
  var ui = {
    tab: 'tree', pane: 'tree', sel: null, picked: null, view: '45',
    ran: {}, saved: false, filter: ''
  };
  var undoStack = [], redoStack = [];
  var runner = { pos: -1, next: 0, from: null, t0: 0, dur: 0 };
  var dragItem = null;

  function newId() { return work.nextId++; }

  function load() {
    try {
      var raw = window.localStorage.getItem(STORE_KEY);
      if (!raw) return;
      var s = JSON.parse(raw);
      if (s && validWork(s.work)) {
        work = s.work;
        ui.sel = s.sel || null;
        ui.ran = s.ran || {};
        ui.view = s.view || '45';
        ui.saved = !!s.saved;
        ui.tab = s.tab === 'run' ? 'run' : 'tree';
      }
    } catch (e) { /* no storage here: start fresh */ }
  }
  function persist() {
    try {
      window.localStorage.setItem(STORE_KEY, JSON.stringify({
        work: work, sel: ui.sel, ran: ui.ran, view: ui.view, saved: ui.saved, tab: ui.tab
      }));
    } catch (e) { /* storage blocked or full: the page still works */ }
  }

  function validWork(w) {
    if (!w || typeof w !== 'object' || !Array.isArray(w.productions)) return false;
    return w.productions.every(function (p) {
      return p && typeof p.name === 'string' && Array.isArray(p.lists) && p.lists.every(function (l) {
        return l && typeof l.name === 'string' && Array.isArray(l.cues) && l.cues.every(function (c) {
          return c && typeof c.name === 'string' && Array.isArray(c.layers) && c.layers.every(function (y) {
            return y && typeof y.group === 'string';
          });
        });
      });
    });
  }
  function cleanWork(w) {
    // Keep only what this page knows, with fresh ids.
    var out = emptyWork();
    w.productions.forEach(function (p) {
      var np = { id: out.nextId++, name: String(p.name), lists: [] };
      if (w.current === p.id) out.current = np.id;
      p.lists.forEach(function (l) {
        var nl = { id: out.nextId++, name: String(l.name), cues: [] };
        l.cues.forEach(function (c) {
          var nc = { id: out.nextId++, name: String(c.name), fade: Math.max(0, Number(c.fade) || 0), layers: [] };
          c.layers.forEach(function (y) {
            if (!groupOf(y.group)) return;
            nc.layers.push({
              id: out.nextId++, group: groupOf(y.group).name,
              colour: palOk(y.colour, 'Colour'), level: palOk(y.level, 'Dimmer'),
              aim: palOk(y.aim, 'Position'), shutter: palOk(y.shutter, 'Shutter')
            });
          });
          nl.cues.push(nc);
        });
        np.lists.push(nl);
      });
      out.productions.push(np);
    });
    if (out.current === null && out.productions.length) out.current = out.productions[0].id;
    return out;
  }
  function palOk(name, type) {
    var p = palOf(name);
    return p && p.type === type ? p.name : null;
  }

  /* Find things by id. */
  function prodById(id) { return work.productions.find(function (p) { return p.id === id; }); }
  function resolve(sel) {
    var r = {};
    if (!sel) return r;
    r.prod = prodById(sel.p);
    if (!r.prod) return {};
    if (sel.l != null) {
      r.list = r.prod.lists.find(function (l) { return l.id === sel.l; });
      if (!r.list) return { prod: r.prod };
    }
    if (sel.c != null && r.list) {
      r.cueIdx = r.list.cues.findIndex(function (c) { return c.id === sel.c; });
      if (r.cueIdx < 0) return { prod: r.prod, list: r.list };
      r.cue = r.list.cues[r.cueIdx];
    }
    if (sel.y != null && r.cue) {
      r.layer = r.cue.layers.find(function (y) { return y.id === sel.y; });
    }
    if (sel.g != null && r.cue) r.group = sel.g;
    return r;
  }
  function selKind() {
    var r = resolve(ui.sel);
    if (r.layer) return 'layer';
    if (r.group && ui.sel.k === 'tracked') return 'tracked';
    if (r.cue) return 'cue';
    if (r.list) return 'list';
    if (r.prod) return 'prod';
    return null;
  }

  /* Cue numbers: a list named "01-…" numbers its cues 1.0.0, 1.0.1… */
  function listMajor(prod, list) {
    var m = /^\s*(\d+)/.exec(list.name);
    if (m) return parseInt(m[1], 10);
    return prod.lists.indexOf(list) + 1;
  }
  function cueNum(prod, list, idx) { return listMajor(prod, list) + '.0.' + idx; }
  function cueLabel(prod, list, idx) {
    var c = list.cues[idx];
    return cueNum(prod, list, idx) + (c.name ? ' ' + c.name : '');
  }

  /* ------------------------------------------------------- tracking -- */

  function membersOf(group) {
    var keys = [];
    group.fixtures.forEach(function (m) {
      var fx = FX.get(m[0]);
      if (!fx) return;
      if (fx.parts) {
        (m[1] === 'all' ? ['rgb', 'w'] : [m[1]]).forEach(function (p) { keys.push(fx.id + ':' + p); });
      } else keys.push(fx.id + ':all');
    });
    return keys;
  }

  // The stage in cue idx of a tracking list: every light's colour, level,
  // aim and shutter, each from the last cue that set it.
  function cueState(list, idx) {
    var st = new Map();
    for (var j = 0; j <= idx && j < list.cues.length; j++) {
      list.cues[j].layers.forEach(function (y) {
        var g = groupOf(y.group);
        if (!g) return;
        membersOf(g).forEach(function (k) {
          var a = st.get(k);
          if (!a) { a = {}; st.set(k, a); }
          ['colour', 'level', 'aim', 'shutter'].forEach(function (at) {
            if (y[at] != null) a[at] = y[at];
          });
        });
      });
    }
    return st;
  }

  // What a group's layer carries into cue idx from earlier cues.
  function trackedInto(list, idx) {
    var own = {};
    if (list.cues[idx]) list.cues[idx].layers.forEach(function (y) { own[y.group.toLowerCase()] = true; });
    var out = new Map();
    for (var j = 0; j < idx; j++) {
      list.cues[j].layers.forEach(function (y) {
        var key = y.group.toLowerCase();
        if (own[key]) return;
        var t = out.get(key) || { group: y.group, vals: {}, from: j };
        ['colour', 'level', 'aim', 'shutter'].forEach(function (at) {
          if (y[at] != null) t.vals[at] = y[at];
        });
        t.from = j;
        out.set(key, t);
      });
    }
    return Array.from(out.values());
  }
  function trackedValsFor(list, idx, groupName) {
    var t = trackedInto(list, idx).find(function (x) { return x.group.toLowerCase() === groupName.toLowerCase(); });
    return t ? t.vals : {};
  }

  function valsText(v) {
    var parts = [];
    if (v.colour) parts.push(v.colour);
    if (v.level) parts.push(v.level);
    if (v.shutter) parts.push(v.shutter);
    if (v.aim) parts.push(v.aim);
    return parts.join(', ');
  }

  /* ---------------------------------------------------- light output -- */

  function hexRgb(h) {
    var n = parseInt(String(h).replace('#', ''), 16);
    return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }

  // Light from one part of a fixture, [r,g,b] in 0..1, or null for dark.
  function partLight(fx, part, a) {
    var cp = palOf(a.colour), lp = palOf(a.level);
    var col = cp ? hexRgb(cp.color) : null;
    var lev = lp ? lp.level : null;
    if (fx.shutterClosed && !a.shutter) return null;
    var c;
    if (part === 'w') {                        // white pixels: the white in the colour
      if (!col) return null;
      var m = Math.min(col[0], col[1], col[2]) * (lev == null ? 1 : lev);
      return m > 0 ? [m, m * 0.96, m * 0.88] : null;
    }
    if (fx.dimmer) {                           // a dimmer: dark until it gets a level
      if (lev == null || lev <= 0) return null;
      c = col || ((fx.colour === 'wheel' || fx.colour === 'white') ? [1, 1, 1] : null);
      if (!c) return null;
      return [c[0] * lev, c[1] * lev, c[2] * lev];
    }
    if (!col) return null;                     // colour only: a level dims the colour
    var l = lev == null ? 1 : lev;
    return l > 0 ? [col[0] * l, col[1] * l, col[2] * l] : null;
  }

  // Every lit fixture: id -> { c: [r,g,b], aim: name|null }.
  function outputsOf(state) {
    var out = new Map();
    SHOW.fixtures.forEach(function (fx) {
      var parts = fx.parts ? ['rgb', 'w'] : ['all'];
      var sum = [0, 0, 0], any = false, aim = null;
      parts.forEach(function (p) {
        var a = state.get(fx.id + ':' + p);
        if (!a) return;
        if (a.aim && fx.moves) aim = a.aim;
        var l = partLight(fx, p, a);
        if (l) { any = true; sum[0] += l[0]; sum[1] += l[1]; sum[2] += l[2]; }
      });
      if (any) out.set(fx.id, { c: sum.map(function (v) { return Math.min(1, v); }), aim: aim });
    });
    return out;
  }
  function cueOutputs(list, idx) {
    if (!list || idx < 0 || idx >= list.cues.length) return new Map();
    return outputsOf(cueState(list, idx));
  }

  // Why a group might stay dark: plain notes for the layer's page.
  function layerNotes(group, vals) {
    var notes = [];
    var fxs = group.fixtures.map(function (m) { return FX.get(m[0]); }).filter(Boolean);
    var withDimmer = fxs.some(function (f) { return f.dimmer; });
    var colourOnly = fxs.some(function (f) { return !f.dimmer; });
    var movers = fxs.some(function (f) { return f.moves; });
    var shut = fxs.some(function (f) { return f.shutterClosed; });
    var whitePart = group.fixtures.every(function (m) { return m[1] === 'w'; });
    if (withDimmer && !vals.level) notes.push('These lights have a dimmer: they stay dark until the layer has a level.');
    if (colourOnly && !vals.colour && !whitePart) notes.push('These lights have no dimmer of their own: they need a colour, and a level dims it.');
    if (whitePart && vals.colour) {
      var c = hexRgb((palOf(vals.colour) || {}).color || '#000000');
      if (Math.min(c[0], c[1], c[2]) < 0.05) notes.push('These are white pixels: a saturated colour like ' + vals.colour + ' leaves them dark. Use the -RGB group for colour.');
    }
    if (shut && !vals.shutter) notes.push('The AT-Spots’ shutter rests closed: add Open (Palettes → Shutter) to let light out.');
    if (vals.aim && !movers) notes.push('These lights can’t move, so the aim does nothing.');
    return notes;
  }

  /* ------------------------------------------------------- undo/edit -- */

  function snapshot() { return JSON.stringify(work); }
  function commit(fn, changed) {
    undoStack.push(snapshot());
    if (undoStack.length > 200) undoStack.shift();
    redoStack.length = 0;
    fn();
    persist();
    render();
    if (changed) status('Changed: ' + changed, true);
  }
  function undo() {
    if (!undoStack.length) { status('Nothing to undo.'); return; }
    redoStack.push(snapshot());
    work = JSON.parse(undoStack.pop());
    fixSelection();
    persist(); render(); status('Undone.', true);
  }
  function redo() {
    if (!redoStack.length) { status('Nothing to redo.'); return; }
    undoStack.push(snapshot());
    work = JSON.parse(redoStack.pop());
    fixSelection();
    persist(); render(); status('Redone.', true);
  }
  function fixSelection() {
    var r = resolve(ui.sel);
    if (!ui.sel) return;
    if (ui.sel.y != null && !r.layer) ui.sel = r.cue ? { k: 'cue', p: r.prod.id, l: r.list.id, c: r.cue.id } : null;
    r = resolve(ui.sel);
    if (ui.sel && ui.sel.c != null && !r.cue) ui.sel = r.list ? { k: 'list', p: r.prod.id, l: r.list.id } : null;
    r = resolve(ui.sel);
    if (ui.sel && ui.sel.l != null && !r.list) ui.sel = r.prod ? { k: 'prod', p: r.prod.id } : null;
    r = resolve(ui.sel);
    if (ui.sel && !r.prod) ui.sel = null;
  }

  function changedFrom(prod, list, idx) {
    var names = [];
    for (var i = idx; i < list.cues.length; i++) names.push(cueLabel(prod, list, i));
    if (names.length > 3) return names.slice(0, 3).join(', ') + ' …';
    return names.join(', ');
  }

  function addProduction(name) {
    name = String(name || '').trim();
    if (!name) return status('A production needs a name.', false, true);
    var id;
    commit(function () {
      id = newId();
      work.productions.push({ id: id, name: name, lists: [] });
      if (work.current == null) work.current = id;
      ui.sel = { k: 'prod', p: id };
    });
    status('Added production ' + name + (work.current === id ? ' (current: GO plays it)' : '') + '.', true);
  }
  function targetProduction() {
    var r = resolve(ui.sel);
    if (r.prod) return r.prod;
    if (work.productions.length === 1) return work.productions[0];
    return null;
  }
  function addList(name) {
    name = String(name || '').trim();
    var prod = targetProduction();
    if (!prod) return status('Click a production in the tree first (or make one with + Production).', false, true);
    if (!name) return status('A cue list needs a name.', false, true);
    commit(function () {
      var id = newId();
      prod.lists.push({ id: id, name: name, cues: [] });
      ui.sel = { k: 'list', p: prod.id, l: id };
    });
    status('Added cue list ' + name + ' (Tracking).', true);
  }
  function targetList() {
    var r = resolve(ui.sel);
    if (r.list) return r;
    var prod = targetProduction();
    if (prod && prod.lists.length === 1) return { prod: prod, list: prod.lists[0] };
    return null;
  }
  function addCue(name, focusName) {
    var r = targetList();
    if (!r) return status('Click a cue list in the tree first (or make one with + Cue list).', false, true);
    var at = r.cue ? r.cueIdx + 1 : r.list.cues.length;
    var id;
    commit(function () {
      id = newId();
      r.list.cues.splice(at, 0, { id: id, name: String(name || '').trim(), fade: 0, layers: [] });
      ui.sel = { k: 'cue', p: r.prod.id, l: r.list.id, c: id };
    }, changedFrom(r.prod, r.list, at));
    if (focusName) {
      setPane('page');
      var inp = document.getElementById('cue-name');
      if (inp) { inp.focus(); inp.select(); }
    }
    return { prod: r.prod, list: r.list, idx: at };
  }
  function renameSelected(name) {
    var r = resolve(ui.sel);
    name = String(name).trim();
    if (r.cue) {
      if (r.cue.name === name) return;
      commit(function () { r.cue.name = name; }, cueLabel(r.prod, r.list, r.cueIdx));
    } else if (r.list) {
      if (!name || r.list.name === name) return;
      commit(function () { r.list.name = name; });
      status('Renamed the cue list.', true);
    } else if (r.prod) {
      if (!name || r.prod.name === name) return;
      commit(function () { r.prod.name = name; });
      status('Renamed the production.', true);
    }
  }
  function addLayer(r, groupName) {
    var g = groupOf(groupName);
    if (!g || !r || !r.cue) return;
    var have = r.cue.layers.find(function (y) { return y.group.toLowerCase() === g.name.toLowerCase(); });
    if (have) {
      ui.sel = { k: 'layer', p: r.prod.id, l: r.list.id, c: r.cue.id, y: have.id };
      render();
      status(g.name + ' is already a layer in ' + cueLabel(r.prod, r.list, r.cueIdx) + '.', true);
      return have;
    }
    var y = { id: 0, group: g.name, colour: null, level: null, aim: null, shutter: null };
    commit(function () {
      y.id = newId();
      r.cue.layers.push(y);
      ui.sel = { k: 'cue', p: r.prod.id, l: r.list.id, c: r.cue.id };
    }, changedFrom(r.prod, r.list, r.cueIdx));
    return y;
  }
  function setAttr(r, layer, attr, value) {
    commit(function () { layer[attr] = value; }, changedFrom(r.prod, r.list, r.cueIdx));
  }
  // A value picked on a grey (tracked) layer: the layer becomes this cue's own.
  function setInThisCue(r, groupName, attr, value) {
    var g = groupOf(groupName);
    if (!g) return;
    var y;
    commit(function () {
      y = { id: newId(), group: g.name, colour: null, level: null, aim: null, shutter: null };
      y[attr] = value;
      r.cue.layers.push(y);
      ui.sel = { k: 'layer', p: r.prod.id, l: r.list.id, c: r.cue.id, y: y.id };
    }, changedFrom(r.prod, r.list, r.cueIdx));
  }
  function deleteSelected() {
    var r = resolve(ui.sel);
    var k = selKind();
    if (!k) return status('Click something in the tree to delete it.', false, true);
    if (k === 'tracked') return status('A grey row comes from an earlier cue: delete it there.', false, true);
    var what;
    commit(function () {
      if (k === 'layer') {
        what = r.layer.group + ' from ' + cueLabel(r.prod, r.list, r.cueIdx);
        r.cue.layers.splice(r.cue.layers.indexOf(r.layer), 1);
        ui.sel = { k: 'cue', p: r.prod.id, l: r.list.id, c: r.cue.id };
      } else if (k === 'cue') {
        what = cueLabel(r.prod, r.list, r.cueIdx);
        r.list.cues.splice(r.cueIdx, 1);
        ui.sel = { k: 'list', p: r.prod.id, l: r.list.id };
      } else if (k === 'list') {
        what = r.list.name;
        r.prod.lists.splice(r.prod.lists.indexOf(r.list), 1);
        ui.sel = { k: 'prod', p: r.prod.id };
      } else {
        what = r.prod.name;
        work.productions.splice(work.productions.indexOf(r.prod), 1);
        if (work.current === r.prod.id) work.current = work.productions.length ? work.productions[0].id : null;
        ui.sel = null;
      }
    });
    status('Deleted ' + what + '. Undo brings it back.', true);
  }

  /* ------------------------------------------------- drag, drop, pick -- */

  // Library items: "g:<group>", "p:<palette>", "t:<target id>".
  function itemLabel(it) {
    var k = it.slice(0, 1), v = it.slice(2);
    if (k === 't') { var t = TARGETS.get(+v); return t ? t.name : v; }
    if (k === 'p') { var p = palOf(v); return p ? p.name : v; }
    var g = groupOf(v); return g ? g.name : v;
  }
  function aimFor(it) {
    var k = it.slice(0, 1), v = it.slice(2);
    if (k === 'p') return palOf(v) && palOf(v).type === 'Position' ? palOf(v).name : null;
    if (k === 't') {
      var p = PAL.Position.find(function (q) { return q.target === +v; });
      return p ? p.name : null;
    }
    return null;
  }
  // Drop targets: "cue|p|l|c", "layer|p|l|c|y", "tracked|p|l|c|group".
  function parseTarget(s) {
    var a = String(s).split('|');
    var sel = { k: a[0], p: +a[1], l: +a[2], c: +a[3] };
    if (a[0] === 'layer') sel.y = +a[4];
    if (a[0] === 'tracked') sel.g = decodeURIComponent(a[4]);
    return sel;
  }
  // What a drop would do: { ok, text, run() }.
  function planDrop(it, tgt) {
    var sel = parseTarget(tgt), r = resolve(sel);
    if (!r.cue) return { ok: false, text: '' };
    var where = cueLabel(r.prod, r.list, r.cueIdx);
    var k = it.slice(0, 1);
    if (k === 'g') {
      var g = groupOf(it.slice(2));
      return {
        ok: true, text: 'a new layer on ' + g.name + ' in ' + where,
        run: function () { addLayer(r, g.name); }
      };
    }
    var attr, value;
    if (k === 't') { attr = 'aim'; value = aimFor(it); }
    else {
      var p = palOf(it.slice(2));
      if (!p) return { ok: false, text: '' };
      attr = ATTR_OF[p.type]; value = p.name;
    }
    if (!value) return { ok: false, text: '' };
    if (sel.k === 'layer' && r.layer) {
      return {
        ok: true, text: value + ' → ' + r.layer.group + ' in ' + where,
        run: function () { setAttr(r, r.layer, attr, value); }
      };
    }
    if (sel.k === 'tracked') {
      return {
        ok: true, text: value + ' → ' + sel.g + ', set in ' + where,
        run: function () { setInThisCue(r, sel.g, attr, value); }
      };
    }
    return { ok: false, text: 'Drop ' + value + ' on a layer row, not the drop area.' };
  }
  function dropOn(it, tgt) {
    var plan = planDrop(it, tgt);
    if (plan.ok) plan.run();
    else status(plan.text || 'That doesn’t go there.', false, true);
    ui.picked = null;
    markPicked();
  }
  // Return in the Library (or a double-click): use the item on the selection.
  function useOnSelection(it) {
    var r = resolve(ui.sel), k = selKind();
    if (!r.cue) return status('Click a cue (or a layer) in the tree first.', false, true);
    if (it[0] === 'g') return addLayer(r, it.slice(2));
    if (k === 'layer') return dropOn(it, ['layer', r.prod.id, r.list.id, r.cue.id, r.layer.id].join('|'));
    if (k === 'tracked') return dropOn(it, ['tracked', r.prod.id, r.list.id, r.cue.id, encodeURIComponent(r.group)].join('|'));
    status('Click a layer row first, then use ' + itemLabel(it) + '.', false, true);
  }
  function markPicked() {
    root.querySelectorAll('.item').forEach(function (el) {
      el.classList.toggle('picked', el.dataset.item === ui.picked);
    });
    root.querySelectorAll('.dropbox').forEach(function (el) {
      el.classList.toggle('ready', !!ui.picked && ui.picked[0] === 'g');
    });
  }

  /* ---------------------------------------------------------- status -- */

  var statusTimer = null;
  function status(text, flash, err) {
    var el = document.getElementById('status');
    if (!el) return;
    el.textContent = text;
    el.title = text;
    el.classList.toggle('flash', !!flash);
    el.classList.toggle('err', !!err);
    clearTimeout(statusTimer);
    statusTimer = setTimeout(function () { el.classList.remove('flash'); }, 1600);
  }

  /* ------------------------------------------------------------ html -- */

  function esc(s) {
    return String(s == null ? '' : s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function shell() {
    root.innerHTML =
      '<div class="notice" role="note">' +
        '<span><b>Practice version</b> of qlcconsole’s Show Tree, in a browser. It is not the real app: it does Lesson 1.1 and sends nothing to real lights. Your work stays in this browser.</span>' +
        '<span class="spacer"></span>' +
        '<button type="button" data-act="save">Save to file</button>' +
        '<button type="button" data-act="open">Open file</button>' +
        '<button type="button" data-act="about">About</button>' +
      '</div>' +
      '<div class="toolbar" id="toolbar">' +
        '<button class="btn" type="button" data-act="add-prod">+ Production</button>' +
        '<button class="btn" type="button" data-act="add-list">+ Cue list</button>' +
        '<button class="btn" type="button" data-act="add-cue">+ Cue</button>' +
        '<span class="sep"></span>' +
        '<button class="btn" type="button" data-act="delete">Delete</button>' +
        '<button class="btn" type="button" data-act="undo" title="Undo (Ctrl+Z / ⌘Z)">Undo</button>' +
        '<button class="btn" type="button" data-act="redo" title="Redo">Redo</button>' +
        '<div class="status" id="status" aria-live="polite"></div>' +
      '</div>' +
      '<div class="view" id="view"></div>' +
      '<div class="cmd" id="cmdrow">' +
        '<label for="cmd">Command</label>' +
        '<input id="cmd" type="text" autocomplete="off" spellcheck="false" placeholder="e.g. TrussC-RGB colour Blue dimmer Full — or help">' +
        '<span class="cmdmsg" id="cmdmsg"></span>' +
      '</div>' +
      '<div class="tabs" role="tablist">' +
        '<button class="tab" type="button" role="tab" data-act="tab" data-tab="tree">Show Tree</button>' +
        '<button class="tab" type="button" role="tab" data-act="tab" data-tab="run">Run</button>' +
        '<span class="key">Space = GO in the Run tab</span>' +
      '</div>';
  }

  function render() {
    renderToolbar();
    var view = document.getElementById('view');
    root.querySelectorAll('.tab').forEach(function (t) { t.classList.toggle('on', t.dataset.tab === ui.tab); });
    document.getElementById('toolbar').hidden = ui.tab !== 'tree';
    document.getElementById('cmdrow').hidden = ui.tab !== 'tree';
    if (ui.tab === 'tree') {
      if (!view.querySelector('.tree-view')) buildTreeView(view);
      view.querySelector('.tree-view').dataset.pane = ui.pane;
      root.querySelectorAll('.panes-switch .tab').forEach(function (t) { t.classList.toggle('on', t.dataset.pane === ui.pane); });
      renderTree();
      renderPage();
      renderSteps();
      markPicked();
      stage.attach(document.getElementById('stagewrap'));
    } else {
      if (!view.querySelector('.run-view')) buildRunView(view);
      renderRun();
      stage.attach(document.getElementById('runstagewrap'));
    }
    stage.draw();
  }

  function renderToolbar() {
    var k = selKind();
    var tb = document.getElementById('toolbar');
    tb.querySelector('[data-act=add-list]').disabled = !targetProduction();
    tb.querySelector('[data-act=add-cue]').disabled = !targetList();
    tb.querySelector('[data-act=delete]').disabled = !k || k === 'tracked';
    tb.querySelector('[data-act=undo]').disabled = !undoStack.length;
    tb.querySelector('[data-act=redo]').disabled = !redoStack.length;
  }

  function buildTreeView(view) {
    view.innerHTML =
      '<div class="tree-view">' +
        '<div class="tabs panes-switch" role="tablist">' +
          '<button class="tab" type="button" data-act="pane" data-pane="lib">Library</button>' +
          '<button class="tab" type="button" data-act="pane" data-pane="tree">Tree</button>' +
          '<button class="tab" type="button" data-act="pane" data-pane="page">Page</button>' +
        '</div>' +
        '<section class="pane lib" aria-label="Library"><h2>Library</h2>' +
          '<input class="filter" id="filter" type="search" placeholder="Filter the Library" autocomplete="off">' +
          '<div class="scroll"><div class="libtree" id="libtree">' + libraryHtml() + '</div></div></section>' +
        '<section class="pane treepane" aria-label="Show tree"><h2>Show</h2>' +
          '<div class="scroll"><div class="tree" id="tree"></div></div></section>' +
        '<section class="pane page" aria-label="Page"><h2 id="pagetitle">Page</h2>' +
          '<div class="scroll"><div class="pagebody" id="pagebody"></div></div></section>' +
        '<section class="pane stagepane" aria-label="Rig Overview"><h2>Rig Overview<span class="right stagebar">' + viewSeg() + '</span></h2>' +
          '<div class="canvaswrap" id="stagewrap"><span class="stagecap" id="stagecap"></span></div>' +
          '<details class="steps" id="steps" open><summary>Lesson 1.1 steps</summary><ol id="steplist"></ol></details>' +
        '</section>' +
      '</div>';
    document.getElementById('filter').value = ui.filter;
    applyFilter();
  }
  function viewSeg() {
    return '<span class="seg">' + [['45', '45°'], ['front', 'Front'], ['top', 'Top']].map(function (v) {
      return '<button type="button" data-act="view" data-view="' + v[0] + '" class="' + (ui.view === v[0] ? 'on' : '') + '">' + v[1] + '</button>';
    }).join('') + '</span>';
  }

  function libraryHtml() {
    function item(key, label, extra, icon) {
      return '<div class="item" draggable="true" tabindex="0" role="button" data-item="' + esc(key) + '" data-name="' + esc(label.toLowerCase()) + '">' +
        (icon || '') + '<span>' + esc(label) + '</span>' + (extra ? ' <span class="n">' + esc(extra) + '</span>' : '') + '</div>';
    }
    function sortName(a, b) { var x = a.name.toLowerCase(), y = b.name.toLowerCase(); return x < y ? -1 : x > y ? 1 : 0; }
    var html = '<details open data-node="all"><summary>All</summary>';
    html += '<details open data-node="groups"><summary>Groups</summary>';
    SHOW.folders.forEach(function (folder) {
      html += '<details data-node="f:' + esc(folder) + '"><summary class="folder">📁 ' + esc(folder) + '</summary>';
      SHOW.groups.filter(function (g) { return g.folder === folder; }).sort(sortName).forEach(function (g) {
        html += item('g:' + g.name, g.name, '(' + g.fixtures.length + ')', '<span class="icon">▦</span>');
      });
      html += '</details>';
    });
    html += '</details>';
    html += '<details data-node="targets"><summary>Targets</summary>';
    SHOW.targets.forEach(function (t) {
      html += item('t:' + t.id, t.name, t.kind === 'line' ? 'line' : '', '<span class="icon">⌖</span>');
    });
    html += '</details>';
    html += '<details open data-node="palettes"><summary>Palettes</summary>';
    ['Colour', 'Dimmer', 'Position', 'Shutter'].forEach(function (type) {
      var list = (PAL[type] || []).slice();
      if (type === 'Colour') list.sort(sortName);
      html += '<details data-node="p:' + type + '"><summary>' + type + '</summary>';
      list.forEach(function (p) {
        var icon = type === 'Colour' ? '<span class="sw" style="background:' + esc(p.color) + '"></span>' :
          type === 'Dimmer' ? '<span class="icon">◐</span>' : type === 'Position' ? '<span class="icon">⌖</span>' : '<span class="icon">◯</span>';
        html += item('p:' + p.name, p.name, '', icon);
      });
      html += '</details>';
    });
    html += '</details></details>';
    return html;
  }
  function applyFilter() {
    var f = ui.filter.trim().toLowerCase();
    var lib = document.getElementById('libtree');
    if (!lib) return;
    lib.querySelectorAll('.item').forEach(function (el) {
      el.hidden = !!f && el.dataset.name.indexOf(f) < 0;
    });
    if (f) {
      lib.querySelectorAll('details').forEach(function (d) {
        var any = d.querySelector('.item:not([hidden])');
        d.hidden = !any;
        if (any) d.open = true;
      });
    } else {
      lib.querySelectorAll('details').forEach(function (d) { d.hidden = false; });
    }
  }

  function renderTree() {
    var el = document.getElementById('tree');
    if (!work.productions.length) {
      el.innerHTML = '<div class="empty">No production yet. Click <b>+ Production</b> to start.</div>';
      return;
    }
    var s = ui.sel || {};
    var html = '';
    work.productions.forEach(function (p) {
      var on = s.k === 'prod' && s.p === p.id;
      html += '<div class="row prod' + (on ? ' sel' : '') + '" data-sel="prod|' + p.id + '"><span class="label">' + esc(p.name) + '</span>' +
        (work.current === p.id ? '<span class="tag">▶ current</span>' : '') + '</div>';
      p.lists.forEach(function (l) {
        var lon = s.k === 'list' && s.p === p.id && s.l === l.id;
        html += '<div class="row d1' + (lon ? ' sel' : '') + '" data-sel="list|' + p.id + '|' + l.id + '"><span class="label">' + esc(l.name) + '</span><span class="tag">Tracking</span></div>';
        l.cues.forEach(function (c, i) {
          var con = s.k === 'cue' && s.c === c.id;
          html += '<div class="row d2' + (con ? ' sel' : '') + '" data-sel="cue|' + p.id + '|' + l.id + '|' + c.id + '" data-drop="cue|' + p.id + '|' + l.id + '|' + c.id + '">' +
            '<span class="label">' + esc(cueLabel(p, l, i)) + '</span></div>';
          c.layers.forEach(function (y) {
            var yon = s.k === 'layer' && s.y === y.id;
            var tgt = 'layer|' + p.id + '|' + l.id + '|' + c.id + '|' + y.id;
            html += '<div class="row d3' + (yon ? ' sel' : '') + '" data-sel="' + tgt + '" data-drop="' + tgt + '"><span class="label">' + esc(y.group) + '</span></div>';
          });
          trackedInto(l, i).forEach(function (t) {
            var tg = 'tracked|' + p.id + '|' + l.id + '|' + c.id + '|' + encodeURIComponent(t.group);
            var ton = s.k === 'tracked' && s.c === c.id && s.g === t.group;
            html += '<div class="row d3 tracked' + (ton ? ' sel' : '') + '" data-sel="' + esc(tg) + '" data-drop="' + esc(tg) + '" title="tracked from ' + esc(cueLabel(p, l, t.from)) + '">' +
              '<span class="label">⤳ ' + esc(t.group) + '</span></div>';
          });
        });
      });
    });
    el.innerHTML = html;
  }

  function chipsHtml(type, attr, current, trackedVal, disabled) {
    var list = PAL[type] || [];
    var html = '<div class="chips">';
    list.forEach(function (p) {
      var cls = 'chip' + (current === p.name ? ' on' : (!current && trackedVal === p.name ? ' tracked' : ''));
      html += '<button type="button" class="' + cls + '" data-act="setattr" data-attr="' + attr + '" data-val="' + esc(p.name) + '"' + (disabled ? ' disabled' : '') + '>' +
        (type === 'Colour' ? '<span class="sw" style="background:' + esc(p.color) + '"></span>' : '') + esc(p.name) + '</button>';
    });
    if (current) html += '<button type="button" class="chip" data-act="setattr" data-attr="' + attr + '" data-val="">Clear</button>';
    return html + '</div>';
  }

  function renderPage() {
    var body = document.getElementById('pagebody'), title = document.getElementById('pagetitle');
    var r = resolve(ui.sel), k = selKind();
    if (!k) {
      title.textContent = 'Page';
      body.innerHTML = welcomeHtml();
      return;
    }
    if (k === 'prod') {
      title.textContent = 'Production';
      body.innerHTML =
        '<div class="field"><label for="nm">Name</label><input id="nm" type="text" data-act="rename" value="' + esc(r.prod.name) + '"></div>' +
        '<div class="field"><span class="k">Current</span><div class="v">' +
          (work.current === r.prod.id ? '▶ current — GO plays this production’s first cue list.' :
            '<button type="button" class="btn" data-act="make-current">Make current</button> <span class="sub">so GO plays it</span>') + '</div></div>' +
        '<div class="field"><span class="k">Cue lists</span><div class="v">' + (r.prod.lists.length ? r.prod.lists.map(function (l) { return esc(l.name); }).join(', ') : '<span class="sub">none yet: click <b>+ Cue list</b></span>') + '</div></div>';
      return;
    }
    if (k === 'list') {
      title.textContent = 'Cue list';
      body.innerHTML =
        '<div class="field"><label for="nm">Name</label><input id="nm" type="text" data-act="rename" value="' + esc(r.list.name) + '"></div>' +
        '<div class="field"><span class="k">Mode</span><div class="v">Tracking <span class="sub">— a cue starts from the one before and changes only what it sets.</span></div></div>' +
        '<div class="field"><span class="k">Cues</span><div class="v">' + (r.list.cues.length ? r.list.cues.length + ' · first is ' + esc(cueNum(r.prod, r.list, 0)) : '<span class="sub">none yet: click <b>+ Cue</b></span>') + '</div></div>' +
        '<div class="field"><span class="k">Numbers</span><div class="v sub">A name that starts with a number numbers its cues: “01-…” starts at 1.0.0.</div></div>';
      return;
    }
    if (k === 'cue') return renderCuePage(r, title, body);
    return renderLayerPage(r, k, title, body);
  }

  function welcomeHtml() {
    var empty = !work.productions.length;
    return '<div class="welcome">' +
      '<h3>Lesson 1.1: from a song to cues</h3>' +
      '<p class="sub">Light the first three moments of “Everybody Scream”: Pose, Verse, Chorus. The steps are on the Lesson 1.1 handout and in the list beside the stage.</p>' +
      '<ol>' +
        '<li><b>+ Production</b>: <code>Spring Show</code></li>' +
        '<li><b>+ Cue list</b>: <code>01-Everybody Scream</code></li>' +
        '<li><b>+ Cue</b> three times: Pose, Verse, Chorus</li>' +
        '<li>Drag a group from the Library onto each cue, then a colour and a level onto its row</li>' +
        '<li>Run tab: <b>GO</b> three times</li>' +
      '</ol>' +
      '<p class="sub">No mouse drag? Tap a Library item, then tap where it goes.</p>' +
      (empty ? '<p class="sub">Want to see where you’re heading first? <button type="button" class="link" data-act="example">Load the finished Lesson 1.1</button> (Undo takes it back).</p>' :
        '<p class="sub">Click something in the tree to see its page.</p>') +
      '</div>';
  }

  function renderCuePage(r, title, body) {
    title.textContent = 'Cue';
    var p = r.prod, l = r.list, i = r.cueIdx, c = r.cue;
    var cueRef = 'cue|' + p.id + '|' + l.id + '|' + c.id;
    var html =
      '<div class="field"><span class="k">Number</span><div class="v">' + esc(cueNum(p, l, i)) + '</div></div>' +
      '<div class="field"><label for="cue-name">Name</label><input id="cue-name" type="text" data-act="rename" value="' + esc(c.name) + '" placeholder="Type a name, then Return"></div>' +
      '<div class="field"><label for="cue-fade">Fade in</label><div class="v"><input id="cue-fade" type="number" min="0" max="60" step="0.5" value="' + (c.fade || 0) + '" style="width:7em"> <span class="sub">s (0 = a snap)</span></div></div>' +
      '<div class="field"><span class="k">Hold</span><div class="v">∞ — waits for GO</div></div>' +
      '<div class="field" style="align-items:start"><span class="k">Layers</span><div class="v layers">';
    c.layers.forEach(function (y) {
      var tgt = 'layer|' + p.id + '|' + l.id + '|' + c.id + '|' + y.id;
      html += '<div class="layerrow" data-sel="' + tgt + '" data-drop="' + tgt + '" tabindex="0"><span class="g">' + esc(y.group) + '</span><span class="vals">' + esc(valsText(y) || '— drop a colour and a level here') + '</span></div>';
    });
    trackedInto(l, i).forEach(function (t) {
      var tg = 'tracked|' + p.id + '|' + l.id + '|' + c.id + '|' + encodeURIComponent(t.group);
      html += '<div class="layerrow tracked" data-sel="' + esc(tg) + '" data-drop="' + esc(tg) + '" tabindex="0"><span class="g">⤳ ' + esc(t.group) + '</span><span class="vals">' +
        esc(valsText(t.vals)) + ' … tracked from ' + esc(cueLabel(p, l, t.from)) + '</span></div>';
    });
    html += '<div class="dropbox" data-drop="' + cueRef + '">Drag groups here to add a layer<br><span class="sub">or tap a group in the Library, then tap here</span></div>';
    html += '<div class="adder"><select id="addgroup" aria-label="Group to add"><option value="">Add a group…</option>' +
      SHOW.folders.map(function (f) {
        return '<optgroup label="' + esc(f) + '">' + SHOW.groups.filter(function (g) { return g.folder === f; }).map(function (g) {
          return '<option value="' + esc(g.name) + '">' + esc(g.name) + '</option>';
        }).join('') + '</optgroup>';
      }).join('') + '</select><button type="button" class="btn" data-act="addgroup">Add</button></div>';
    html += '</div></div>';
    body.innerHTML = html;
  }

  function renderLayerPage(r, k, title, body) {
    var p = r.prod, l = r.list, i = r.cueIdx;
    var groupName = k === 'layer' ? r.layer.group : r.group;
    var g = groupOf(groupName);
    var own = k === 'layer' ? r.layer : {};
    var tracked = trackedValsFor(l, i, groupName);
    var eff = {};
    ['colour', 'level', 'aim', 'shutter'].forEach(function (a) { eff[a] = own[a] || tracked[a] || null; });
    var fxs = g.fixtures.map(function (m) { return FX.get(m[0]); }).filter(Boolean);
    var movers = fxs.some(function (f) { return f.moves; });
    var shut = fxs.some(function (f) { return f.shutterClosed; });
    title.textContent = 'Layer';
    var html = '<div><div style="font-size:16px;font-weight:600">' + esc(g.name) + ' <span class="sub" style="font-weight:400">in ' + esc(cueLabel(p, l, i)) + '</span></div>' +
      '<div class="sub">' + fxs.length + ' light' + (fxs.length === 1 ? '' : 's') + ' · ' + esc(GROUP_ABOUT[g.name.toLowerCase()] || '') + '</div></div>';
    if (k === 'tracked') {
      var t = trackedInto(l, i).find(function (x) { return x.group === groupName; });
      html += '<div class="hint">⤳ Tracked from ' + esc(cueLabel(p, l, t ? t.from : 0)) + ': it stays as it was until you change it. Pick a value to set it in this cue.</div>';
    }
    html += '<div class="field" style="align-items:start"><span class="k">Colour</span>' + chipsHtml('Colour', 'colour', own.colour, tracked.colour) + '</div>';
    html += '<div class="field" style="align-items:start"><span class="k">Level</span>' + chipsHtml('Dimmer', 'level', own.level, tracked.level) + '</div>';
    html += '<div class="field" style="align-items:start"><span class="k">Aim</span><div>' + chipsHtml('Position', 'aim', own.aim, tracked.aim, !movers) +
      (movers ? '' : '<div class="sub">These lights don’t move.</div>') + '</div></div>';
    if (shut || own.shutter || tracked.shutter) html += '<div class="field" style="align-items:start"><span class="k">Shutter</span>' + chipsHtml('Shutter', 'shutter', own.shutter, tracked.shutter) + '</div>';
    layerNotes(g, eff).forEach(function (n) { html += '<div class="hint">' + esc(n) + '</div>'; });
    if (k === 'layer') html += '<div><button type="button" class="btn" data-act="delete">Remove this layer</button> <button type="button" class="btn" data-act="to-cue">Back to the cue</button></div>';
    else html += '<div><button type="button" class="btn" data-act="to-cue">Back to the cue</button></div>';
    body.innerHTML = html;
  }

  /* Lesson 1.1 steps, ticked as they're done. */
  function lessonSteps() {
    var prod = work.productions.find(function (p) { return /^spring show$/i.test(p.name.trim()); });
    var list = prod && prod.lists.find(function (l) { return /^0?1\s*-\s*everybody scream$/i.test(l.name.trim()); });
    var cues = list ? list.cues : [];
    function cue(i, name) { return cues[i] && cues[i].name.trim().toLowerCase() === name ? cues[i] : null; }
    function layer(c, g) { return c && c.layers.find(function (y) { return y.group.toLowerCase() === g; }); }
    function looks(y, col, lev) { return y && y.colour === col && y.level === lev; }
    var pose = cue(0, 'pose'), verse = cue(1, 'verse'), chorus = cue(2, 'chorus');
    var ranAll = pose && verse && chorus && ui.ran[pose.id] && ui.ran[verse.id] && ui.ran[chorus.id];
    return [
      ['+ Production: Spring Show', !!prod],
      ['+ Cue list: 01-Everybody Scream', !!list],
      ['+ Cue: 1.0.0 Pose', !!pose],
      ['Pose: drag TrussC-RGB onto the cue', !!layer(pose, 'trussc-rgb')],
      ['TrussC-RGB: Blue, Full', looks(layer(pose, 'trussc-rgb'), 'Blue', 'Full')],
      ['+ Cue: 1.0.1 Verse (the cyc tracks on, grey)', !!verse],
      ['Verse: StepTops, Warm White, 75%', looks(layer(verse, 'steptops'), 'Warm White', '75%')],
      ['+ Cue: 1.0.2 Chorus', !!chorus],
      ['Chorus: Steps, Magenta, Full', looks(layer(chorus, 'steps'), 'Magenta', 'Full')],
      ['Run tab: GO through all three', !!ranAll],
      ['Save to file, with your name', !!ui.saved]
    ];
  }
  function renderSteps() {
    var el = document.getElementById('steplist');
    if (!el) return;
    var steps = lessonSteps();
    el.innerHTML = steps.map(function (s) { return '<li class="' + (s[1] ? 'done' : '') + '">' + esc(s[0]) + '</li>'; }).join('');
    var n = steps.filter(function (s) { return s[1]; }).length;
    var sum = document.querySelector('#steps summary');
    if (sum) sum.textContent = 'Lesson 1.1 steps · ' + n + ' of ' + steps.length;
  }

  /* --------------------------------------------------------- the Run tab -- */

  function runList() {
    var prod = prodById(work.current);
    if (!prod || !prod.lists.length) return null;
    return { prod: prod, list: prod.lists[0] };
  }

  function buildRunView(view) {
    view.innerHTML =
      '<div class="run-view">' +
        '<div class="rhead"><h2 id="runtitle"></h2><span class="mode">DESIGN</span></div>' +
        '<section class="pane rlist" aria-label="Cues"><div class="scroll"><table class="cuetable"><thead><tr><th></th><th>Cue</th><th>Name</th><th>Timing</th></tr></thead><tbody id="cuebody"></tbody></table></div></section>' +
        '<section class="pane rstage" aria-label="Rig Overview"><h2>Rig Overview<span class="right stagebar">' + viewSeg() + '</span></h2>' +
          '<div class="canvaswrap" id="runstagewrap"><span class="stagecap" id="runcap"></span></div></section>' +
        '<div class="rnext"><div class="state" id="runstate"></div><div class="what" id="runwhat"></div></div>' +
        '<div class="rgo">' +
          '<button type="button" class="sidebtn" data-act="back">Back</button>' +
          '<button type="button" class="gobtn" id="gobtn" data-act="go">GO</button>' +
          '<button type="button" class="sidebtn" data-act="stop">Stop</button>' +
        '</div>' +
      '</div>';
  }

  function fadeProgress() {
    if (!runner.dur) return 1;
    return Math.max(0, Math.min(1, (performance.now() - runner.t0) / (runner.dur * 1000)));
  }

  function renderRun() {
    var rl = runList();
    var title = document.getElementById('runtitle'), body = document.getElementById('cuebody');
    var go = document.getElementById('gobtn');
    if (!rl) {
      title.textContent = 'Nothing to run yet';
      body.innerHTML = '<tr><td colspan="4" class="empty">GO plays the current production’s first cue list. Make one in the Show Tree tab: + Production, + Cue list, + Cue.</td></tr>';
      document.getElementById('runstate').textContent = 'Stopped';
      document.getElementById('runwhat').textContent = '—';
      go.disabled = true;
      return;
    }
    var prod = rl.prod, list = rl.list;
    if (runner.pos >= list.cues.length) runner.pos = list.cues.length - 1;
    if (runner.next > list.cues.length) runner.next = list.cues.length;
    title.textContent = prod.name + ' · ' + list.name;
    var prog = fadeProgress();
    body.innerHTML = list.cues.map(function (c, i) {
      var cls = i === runner.pos ? 'live' : i === runner.next ? 'next' : i < runner.pos ? 'done' : '';
      var st = i === runner.pos ? '▶' + (prog < 1 ? '<div class="bar"><i style="width:' + Math.round(prog * 100) + '%"></i></div>' : '') :
        i === runner.next ? 'NEXT' : '';
      return '<tr class="' + cls + '" data-act="load" data-idx="' + i + '"><td class="state">' + st + '</td><td>' + esc(cueNum(prod, list, i)) + '</td><td>' + esc(c.name) + '</td><td>' + (c.fade ? esc(c.fade) + ' s' : '') + '</td></tr>';
    }).join('') || '<tr><td colspan="4" class="empty">This cue list has no cues yet.</td></tr>';
    document.getElementById('runstate').textContent = runner.pos < 0 ? 'Stopped' :
      '▶ ' + cueLabel(prod, list, runner.pos) + (prog < 1 ? ' — fading' : '');
    document.getElementById('runwhat').textContent = whatChanges(prod, list);
    go.disabled = runner.next >= list.cues.length;
  }

  // "NEXT 1.0.1 Verse: StepTops in → Warm White, 75%"
  function whatChanges(prod, list) {
    var n = runner.next;
    if (n >= list.cues.length) return list.cues.length ? 'End of the list. Back steps back; Stop goes dark.' : '—';
    var before = runner.pos >= 0 ? cueState(list, runner.pos) : new Map();
    var bits = list.cues[n].layers.map(function (y) {
      var g = groupOf(y.group);
      var was = g && membersOf(g).some(function (k) { return before.has(k); });
      return y.group + (was ? ' → ' : ' in → ') + (valsText(y) || '(no look yet)');
    });
    return 'NEXT ' + cueLabel(prod, list, n) + ': ' + (bits.length ? bits.join('; ') : 'no change');
  }

  function liveOutputs() {
    var rl = runList();
    var target = rl && runner.pos >= 0 ? cueOutputs(rl.list, runner.pos) : new Map();
    var p = fadeProgress();
    if (p >= 1 || !runner.from) return target;
    return mixOutputs(runner.from, target, p);
  }
  function mixOutputs(a, b, t) {
    var out = new Map();
    var ids = new Set(Array.from(a.keys()).concat(Array.from(b.keys())));
    ids.forEach(function (id) {
      var x = a.get(id), y = b.get(id);
      var cx = x ? x.c : [0, 0, 0], cy = y ? y.c : [0, 0, 0];
      var c = [0, 1, 2].map(function (k) { return cx[k] + (cy[k] - cx[k]) * t; });
      out.set(id, { c: c, aim: (y && y.aim) || (x && x.aim) || null, aimFrom: x && x.aim, aimTo: y && y.aim, t: t });
    });
    return out;
  }
  function startFade(toPos) {
    var rl = runList();
    if (!rl) return;
    runner.from = liveOutputs();
    runner.pos = toPos;
    var cue = rl.list.cues[toPos];
    runner.dur = cue ? Math.max(0, Number(cue.fade) || 0) : 0;
    runner.t0 = performance.now();
    if (cue) ui.ran[cue.id] = true;
    persist();
    animate();
  }
  function go() {
    var rl = runList();
    if (!rl || runner.next >= rl.list.cues.length) return;
    var n = runner.next;
    startFade(n);
    runner.next = n + 1;
    render();
  }
  function back() {
    var rl = runList();
    if (!rl) return;
    if (runner.pos <= 0) return stop();
    var n = runner.pos - 1;
    startFade(n);
    runner.next = n + 1;
    render();
  }
  function stop() {
    runner.from = liveOutputs();
    runner.pos = -1; runner.next = 0; runner.dur = 1; runner.t0 = performance.now();
    animate();
    render();
  }
  function loadCue(i, jump) {
    if (jump) {
      runner.from = null; runner.dur = 0;
      runner.pos = i; runner.next = i + 1;
      var rl = runList();
      if (rl && rl.list.cues[i]) ui.ran[rl.list.cues[i].id] = true;
    } else runner.next = i;
    render();
  }
  var animating = false;
  function animate() {
    if (animating) return;
    animating = true;
    (function frame() {
      var p = fadeProgress();
      if (ui.tab === 'run') { stage.draw(); updateRunProgress(p); }
      if (p < 1) requestAnimationFrame(frame);
      else { animating = false; runner.from = null; if (ui.tab === 'run') render(); }
    })();
  }
  function updateRunProgress(p) {
    var bar = root.querySelector('.cuetable tr.live .bar > i');
    if (bar) bar.style.width = Math.round(p * 100) + '%';
  }

  /* ------------------------------------------------------- the stage -- */

  var stage = (function () {
    var canvas = document.createElement('canvas');
    var ctx = canvas.getContext('2d');
    var holder = null, W = 0, H = 0, dpr = 1, camera = null;
    var ro = window.ResizeObserver ? new ResizeObserver(function () { resize(); draw(); }) : null;

    function attach(el) {
      if (!el || holder === el) return;
      if (holder && ro) ro.unobserve(holder);
      holder = el;
      el.insertBefore(canvas, el.firstChild);
      if (ro) ro.observe(el);
      resize();
    }
    function resize() {
      if (!holder) return;
      var r = holder.getBoundingClientRect();
      dpr = Math.min(2, window.devicePixelRatio || 1);
      W = Math.max(10, r.width); H = Math.max(10, r.height);
      canvas.width = Math.round(W * dpr); canvas.height = Math.round(H * dpr);
      camera = makeCamera(ui.view, W, H);
    }

    function sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
    function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    function norm(a) { var l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; }

    function makeCamera(view, w, h) {
      var cx = CENTRE_X;
      var C, T;
      if (view === 'front') { C = [cx, 22, 1.9]; T = [cx, 4, 2.2]; }
      else if (view === 'top') { C = [cx, 9.5, 26]; T = [cx, 5.3, 0]; }
      else { C = [cx, 21, 5.2]; T = [cx, 4, 2.0]; }
      var f = norm(sub(T, C));
      var up = [0, 0, 1];
      var u = norm(sub(up, f.map(function (v) { return v * dot(up, f); })));
      var r = [1, 0, 0];
      var cam = { C: C, f: f, u: u, r: r, k: 1, ox: 0, oy: 0 };
      // Fit the stage: floor, cyc and towers.
      var pts = [];
      var cyc = SHOW.stage.cyc || { x: cx, y: 1.2, w: 11, h: 6 };
      [[cx - 7.6, cyc.y, 0], [cx + 7.6, cyc.y, 0], [cx - 7.6, 9.3, 0], [cx + 7.6, 9.3, 0],
        [cyc.x - cyc.w / 2, cyc.y, Math.min(cyc.h, 5.6)], [cyc.x + cyc.w / 2, cyc.y, Math.min(cyc.h, 5.6)],
        [cx - 7, 8.7, 2.6], [cx + 7, 8.7, 2.6]].forEach(function (p) { pts.push(raw(cam, p)); });
      var minx = Math.min.apply(null, pts.map(function (p) { return p[0]; }));
      var maxx = Math.max.apply(null, pts.map(function (p) { return p[0]; }));
      var miny = Math.min.apply(null, pts.map(function (p) { return p[1]; }));
      var maxy = Math.max.apply(null, pts.map(function (p) { return p[1]; }));
      var k = Math.min(w * 0.94 / (maxx - minx), h * 0.9 / (maxy - miny));
      cam.k = k;
      cam.ox = w / 2 - k * (minx + maxx) / 2;
      cam.oy = h / 2 - k * (miny + maxy) / 2 + h * 0.02;
      return cam;
    }
    function raw(cam, p) {
      var d = sub(p, cam.C);
      var z = Math.max(0.1, dot(d, cam.f));
      return [dot(d, cam.r) / z, -dot(d, cam.u) / z, z];
    }
    function P(p) {
      var q = raw(camera, p);
      return [camera.ox + camera.k * q[0], camera.oy + camera.k * q[1]];
    }
    function poly(pts, fill, stroke) {
      ctx.beginPath();
      pts.forEach(function (p, i) { var s = P(p); if (i) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]); });
      ctx.closePath();
      if (fill) { ctx.fillStyle = fill; ctx.fill(); }
      if (stroke) { ctx.strokeStyle = stroke; ctx.stroke(); }
    }
    function rgba(c, a) {
      return 'rgba(' + Math.round(c[0] * 255) + ',' + Math.round(c[1] * 255) + ',' + Math.round(c[2] * 255) + ',' + a.toFixed(3) + ')';
    }
    function bright(c) { return Math.max(c[0], c[1], c[2]); }
    function full(c) { var m = bright(c) || 1; return [c[0] / m, c[1] / m, c[2] / m]; }
    function scrDist(a, b) { var p = P(a), q = P(b); return Math.hypot(p[0] - q[0], p[1] - q[1]); }

    function glow(p, radius, c, a) {
      var s = P(p);
      var g = ctx.createRadialGradient(s[0], s[1], 0, s[0], s[1], radius);
      g.addColorStop(0, rgba(full(c), a));
      g.addColorStop(1, rgba(full(c), 0));
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(s[0], s[1], radius, 0, Math.PI * 2); ctx.fill();
    }
    function pool(p, rWorld, c, a) {
      var s = P(p);
      var rx = Math.max(2, scrDist(p, [p[0] + rWorld, p[1], p[2]]));
      var ry = Math.max(1.5, scrDist(p, [p[0], p[1] + rWorld, p[2]]));
      ctx.save();
      ctx.translate(s[0], s[1]); ctx.scale(1, ry / rx);
      var g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
      g.addColorStop(0, rgba(full(c), a));
      g.addColorStop(0.6, rgba(full(c), a * 0.5));
      g.addColorStop(1, rgba(full(c), 0));
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(0, 0, rx, 0, Math.PI * 2); ctx.fill();
      ctx.restore();
    }
    function beam(from, to, rEnd, c, a) {
      var s = P(from), e = P(to);
      var dx = e[0] - s[0], dy = e[1] - s[1];
      var len = Math.hypot(dx, dy) || 1;
      var nx = -dy / len, ny = dx / len;
      var we = Math.max(2, scrDist(to, [to[0] + rEnd, to[1], to[2]]));
      var ws = Math.max(1, we * 0.06);
      var g = ctx.createLinearGradient(s[0], s[1], e[0], e[1]);
      g.addColorStop(0, rgba(full(c), a));
      g.addColorStop(1, rgba(full(c), a * 0.25));
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(s[0] + nx * ws, s[1] + ny * ws);
      ctx.lineTo(e[0] + nx * we, e[1] + ny * we);
      ctx.lineTo(e[0] - nx * we, e[1] - ny * we);
      ctx.lineTo(s[0] - nx * ws, s[1] - ny * ws);
      ctx.closePath(); ctx.fill();
    }

    function targetPoint(name, fx, index, count) {
      var pal = palOf(name);
      var t = pal ? TARGETS.get(pal.target) : null;
      if (!t) return null;
      if (t.line) {
        var a = t.line[0], b = t.line[t.line.length - 1];
        var f = count > 1 ? index / (count - 1) : 0.5;
        return [a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f];
      }
      return [t.x, t.y, t.z + (t.height ? 1.1 : 0)];
    }
    function defaultAim(fx) {
      switch (fx.role) {
        case 'trussspot': return [fx.x, fx.y + 0.5, 0.61];
        case 'ust': return [fx.x, fx.y + 0.3, 8];
        case 'atwash': return [CENTRE_X + (fx.x < CENTRE_X ? -1.0 : 1.0), 6.75, 0.6];
        case 'atspot': return [fx.x + (fx.x < CENTRE_X ? 2.2 : -2.2), 7.9, 0];
        default: return [fx.x, fx.y + 2, 0];
      }
    }
    function aimPoints(outs) {
      // Spread the lights aimed at a line along it, stage right to left.
      var pts = new Map(), byAim = new Map();
      outs.forEach(function (o, id) {
        var fx = FX.get(id);
        if (!fx || !fx.moves) return;
        if (o.aim) {
          if (!byAim.has(o.aim)) byAim.set(o.aim, []);
          byAim.get(o.aim).push(fx);
        } else pts.set(id, defaultAim(fx));
      });
      byAim.forEach(function (list, name) {
        list.sort(function (a, b) { return a.x - b.x; });
        list.forEach(function (fx, i) { pts.set(fx.id, targetPoint(name, fx, i, list.length) || defaultAim(fx)); });
      });
      return pts;
    }

    function figure(x, y, z, h, light) {
      var base = P([x, y, z]), top = P([x, y, z + h]);
      var w = scrDist([x, y, z + h * 0.6], [x + 0.23, y, z + h * 0.6]);
      var c = [0.18 + light[0] * 0.8, 0.19 + light[1] * 0.8, 0.21 + light[2] * 0.8];
      ctx.fillStyle = rgba(c.map(function (v) { return Math.min(1, v); }), 1);
      var hh = base[1] - top[1];
      ctx.beginPath();
      ctx.moveTo(base[0] - w * 0.55, base[1]);
      ctx.lineTo(base[0] - w, base[1] - hh * 0.72);
      ctx.lineTo(base[0] + w, base[1] - hh * 0.72);
      ctx.lineTo(base[0] + w * 0.55, base[1]);
      ctx.closePath(); ctx.fill();
      ctx.beginPath(); ctx.arc(top[0], top[1] + hh * 0.11, Math.max(2, hh * 0.1), 0, Math.PI * 2); ctx.fill();
    }

    function draw() {
      if (!holder || !camera) return;
      var outs, cap;
      if (ui.tab === 'run') {
        outs = liveOutputs();
        var rl = runList();
        cap = 'Run · ' + (rl && runner.pos >= 0 ? '▶ ' + cueLabel(rl.prod, rl.list, runner.pos) : 'stopped, dark');
      } else {
        var r = resolve(ui.sel);
        if (r.cue) { outs = cueOutputs(r.list, r.cueIdx); cap = 'Design · shows ' + cueLabel(r.prod, r.list, r.cueIdx); }
        else { outs = new Map(); cap = 'Design · click a cue to see it here'; }
      }
      var capEl = holder.querySelector('.stagecap');
      if (capEl) capEl.textContent = cap;

      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = '#0b0c10';
      ctx.fillRect(0, 0, W, H);
      var st = SHOW.stage, cx = CENTRE_X;
      var cyc = st.cyc;

      // Floor and grid.
      poly([[cx - 8.3, cyc.y - 0.3, 0], [cx + 8.3, cyc.y - 0.3, 0], [cx + 8.3, 9.6, 0], [cx - 8.3, 9.6, 0]], '#16171c');
      ctx.lineWidth = 1;
      ctx.strokeStyle = 'rgba(120,130,150,0.13)';
      ctx.beginPath();
      for (var gx = Math.ceil(cx - 8); gx <= cx + 8; gx++) { var a1 = P([gx, cyc.y - 0.3, 0]), b1 = P([gx, 9.6, 0]); ctx.moveTo(a1[0], a1[1]); ctx.lineTo(b1[0], b1[1]); }
      for (var gy = Math.ceil(cyc.y); gy <= 9.6; gy++) { var a2 = P([cx - 8.3, gy, 0]), b2 = P([cx + 8.3, gy, 0]); ctx.moveTo(a2[0], a2[1]); ctx.lineTo(b2[0], b2[1]); }
      ctx.stroke();

      // The cyc and the light on it.
      var cycH = Math.min(cyc.h, 5.6);
      var cycPts = [[cyc.x - cyc.w / 2, cyc.y, 0], [cyc.x + cyc.w / 2, cyc.y, 0], [cyc.x + cyc.w / 2, cyc.y, cycH], [cyc.x - cyc.w / 2, cyc.y, cycH]];
      poly(cycPts, '#2a2c33');
      ctx.save();
      poly(cycPts); ctx.clip();
      ctx.globalCompositeOperation = 'lighter';
      outs.forEach(function (o, id) {
        var fx = FX.get(id);
        if (!fx || fx.role !== 'trusscyc') return;
        var b = bright(o.c);
        var top = [fx.x, cyc.y, cycH * 0.82];
        var s = P(top);
        var rx = scrDist(top, [fx.x + 2.6, cyc.y, cycH * 0.82]);
        var ry = scrDist(top, [fx.x, cyc.y, cycH * 0.82 - 3.4]);
        ctx.save(); ctx.translate(s[0], s[1]); ctx.scale(1, ry / rx);
        var g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
        g.addColorStop(0, rgba(full(o.c), 0.95 * b));
        g.addColorStop(0.55, rgba(full(o.c), 0.5 * b));
        g.addColorStop(1, rgba(full(o.c), 0));
        ctx.fillStyle = g; ctx.beginPath(); ctx.arc(0, 0, rx, 0, Math.PI * 2); ctx.fill();
        ctx.restore();
      });
      ctx.restore();

      // Trusses.
      st.trusses.forEach(function (t) {
        var hw = t.w / 2;
        poly([[t.x - hw, t.y, 0], [t.x + hw, t.y, 0], [t.x + hw, t.y, t.h], [t.x - hw, t.y, t.h]], '#5b606b');
        poly([[t.x - hw, t.y, 0.61], [t.x + hw, t.y, 0.61], [t.x + hw, t.y, t.h - 0.05], [t.x - hw, t.y, t.h - 0.05]], '#8b8f98');
      });

      ctx.globalCompositeOperation = 'source-over';
      // Fixture bodies upstage (trusses, USTs).
      SHOW.fixtures.forEach(function (fx) {
        if (fx.y > 4.2 || fx.role === 'step' || fx.role === 'steptop') return;
        var s = P([fx.x, fx.y, fx.z]);
        var sz = Math.max(2.5, scrDist([fx.x, fx.y, fx.z], [fx.x + 0.14, fx.y, fx.z]));
        ctx.fillStyle = '#3b3f48';
        ctx.fillRect(s[0] - sz, s[1] - sz, sz * 2, sz * 2);
      });

      // Risers, upstage first.
      st.risers.slice().sort(function (a, b) { return a.y - b.y || a.z - b.z; }).forEach(function (r) {
        var z0 = r.z, z1 = r.z + r.h;
        poly([[r.x, r.y + r.d, z0], [r.x + r.w, r.y + r.d, z0], [r.x + r.w, r.y + r.d, z1], [r.x, r.y + r.d, z1]], '#1d1d22', 'rgba(0,0,0,0.5)');
        poly([[r.x, r.y, z1], [r.x + r.w, r.y, z1], [r.x + r.w, r.y + r.d, z1], [r.x, r.y + r.d, z1]], '#2a2a31', 'rgba(0,0,0,0.45)');
      });

      // Light on stage (additive).
      ctx.globalCompositeOperation = 'lighter';
      var aims = aimPoints(outs);
      var personLight = SHOW.targets.filter(function (t) { return t.kind === 'person'; }).map(function () { return [0, 0, 0]; });
      var persons = SHOW.targets.filter(function (t) { return t.kind === 'person'; });
      function lightPersons(p, c, amount, reach) {
        persons.forEach(function (t, i) {
          var d = Math.hypot(t.x - p[0], t.y - p[1]);
          if (d < reach) { var f = amount * (1 - d / reach); personLight[i] = personLight[i].map(function (v, k) { return v + c[k] * f; }); }
        });
      }
      var front = [];
      outs.forEach(function (o, id) {
        var fx = FX.get(id);
        if (!fx) return;
        var c = o.c, b = bright(c);
        if (b <= 0.002) return;
        if (fx.y > 8) { front.push([fx, o]); return; }
        switch (fx.role) {
          case 'step': {
            var a = P([fx.x - fx.len / 2, fx.y, fx.z]), e = P([fx.x + fx.len / 2, fx.y, fx.z]);
            ctx.lineCap = 'round';
            ctx.strokeStyle = rgba(full(c), 0.25 * b); ctx.lineWidth = 9;
            ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(e[0], e[1]); ctx.stroke();
            ctx.strokeStyle = rgba(full(c), 0.95 * b); ctx.lineWidth = 2.5;
            ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(e[0], e[1]); ctx.stroke();
            break;
          }
          case 'steptop': {
            var a2 = P([fx.x - fx.len / 2, fx.y, fx.z]), e2 = P([fx.x + fx.len / 2, fx.y, fx.z]);
            var up = scrDist([fx.x, fx.y, fx.z], [fx.x, fx.y, fx.z + 1.3]);
            var g = ctx.createLinearGradient(0, a2[1], 0, a2[1] - up);
            g.addColorStop(0, rgba(full(c), 0.42 * b)); g.addColorStop(1, rgba(full(c), 0));
            ctx.fillStyle = g;
            ctx.fillRect(Math.min(a2[0], e2[0]), a2[1] - up, Math.abs(e2[0] - a2[0]), up);
            pool([fx.x, fx.y + 0.35, fx.z], 0.7, c, 0.35 * b);
            ctx.strokeStyle = rgba(full(c), 0.9 * b); ctx.lineWidth = 2;
            ctx.beginPath(); ctx.moveTo(a2[0], a2[1]); ctx.lineTo(e2[0], e2[1]); ctx.stroke();
            lightPersons([fx.x, fx.y], c, 0.35, 1.6);
            break;
          }
          case 'trusscyc':
            glow([fx.x, fx.y, fx.z], 7, c, 0.9 * b);
            break;
          case 'trusswash': {
            var to = [fx.x + (cx - fx.x) * 0.25, fx.y + 3.4, 0.4];
            beam([fx.x, fx.y, fx.z], to, 1.6, c, 0.12 * b);
            pool(to, 2.2, c, 0.28 * b);
            glow([fx.x, fx.y, fx.z], 7, c, 0.9 * b);
            lightPersons(to, c, 0.35, 3.2);
            break;
          }
          case 'lantern':
            glow([fx.x, fx.y, fx.z], 9, c, 0.95 * b);
            break;
          default: {
            var aim = aims.get(id);
            if (!aim) { glow([fx.x, fx.y, fx.z], 6, c, 0.8 * b); break; }
            drawBeam(fx, aim, c, b);
            if (fx.role !== 'ust') lightPersons(aim, c, 0.9, 1.1);
          }
        }
      });

      ctx.globalCompositeOperation = 'source-over';
      persons.forEach(function (t, i) {
        figure(t.x, t.y, t.z, t.height || 1.65, personLight[i].map(function (v) { return Math.min(1, v); }));
      });

      // Main drape (open) and the proscenium.
      var dr = st.drape;
      if (dr) {
        var dh = st.proscenium ? st.proscenium.openH || dr.h : dr.h;
        var half = (dr.openW || 0) / 2;
        poly([[dr.x - dr.w / 2, dr.y, 0], [dr.x - half, dr.y, 0], [dr.x - half, dr.y, dh], [dr.x - dr.w / 2, dr.y, dh]], '#4a1219');
        poly([[dr.x + half, dr.y, 0], [dr.x + dr.w / 2, dr.y, 0], [dr.x + dr.w / 2, dr.y, dh], [dr.x + half, dr.y, dh]], '#4a1219');
      }
      var pr = st.proscenium;
      if (pr && ui.view !== 'top') {
        var ow = pr.openW / 2, oh = pr.openH;
        ctx.beginPath();
        [[pr.x - pr.w / 2, pr.y, 0], [pr.x + pr.w / 2, pr.y, 0], [pr.x + pr.w / 2, pr.y, pr.h], [pr.x - pr.w / 2, pr.y, pr.h]].forEach(function (p, i) {
          var s = P(p); if (i) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]);
        });
        ctx.closePath();
        [[pr.x - ow, pr.y, 0], [pr.x - ow, pr.y, oh], [pr.x + ow, pr.y, oh], [pr.x + ow, pr.y, 0]].forEach(function (p, i) {
          var s = P(p); if (i) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]);
        });
        ctx.closePath();
        ctx.fillStyle = '#1c1b20';
        ctx.fill('evenodd');
      }

      // Audience towers and their lights.
      st.towers.forEach(function (t) {
        poly([[t.x, t.y + t.d, 0], [t.x + t.w, t.y + t.d, 0], [t.x + t.w, t.y + t.d, t.h], [t.x, t.y + t.d, t.h]], '#30353f', '#4d5464');
      });
      SHOW.fixtures.forEach(function (fx) {
        if (fx.y <= 8) return;
        var s = P([fx.x, fx.y, fx.z]);
        ctx.fillStyle = '#4a4f5a';
        ctx.fillRect(s[0] - 3, s[1] - 3, 6, 6);
      });
      ctx.globalCompositeOperation = 'lighter';
      front.forEach(function (pair) {
        var fx = pair[0], o = pair[1], b = bright(o.c);
        if (fx.role === 'lantern') { glow([fx.x, fx.y, fx.z], 9, o.c, 0.95 * b); return; }
        var aim = aims.get(fx.id);
        if (aim) drawBeam(fx, aim, o.c, b);
        else glow([fx.x, fx.y, fx.z], 6, o.c, 0.8 * b);
      });
      // Light on the singers from the front.
      ctx.globalCompositeOperation = 'source-over';
      if (front.length) {
        persons.forEach(function (t, i) {
          var add = [0, 0, 0];
          front.forEach(function (pair) {
            var aim = aims.get(pair[0].id);
            if (!aim) return;
            var d = Math.hypot(aim[0] - t.x, aim[1] - t.y);
            var reach = pair[0].role === 'atwash' ? 3.2 : 1.0;
            if (d < reach) add = add.map(function (v, k) { return v + pair[1].c[k] * (1 - d / reach); });
          });
          if (bright(add) > 0) figure(t.x, t.y, t.z, t.height || 1.65, personLight[i].map(function (v, k) { return Math.min(1, v + add[k]); }));
        });
      }

      // Labels: which side is which.
      ctx.globalCompositeOperation = 'source-over';
      ctx.fillStyle = 'rgba(200,206,218,0.55)';
      ctx.font = '11px ' + getComputedStyle(document.body).fontFamily;
      var l1 = P([cx - 7.2, 9.4, 0]), l2 = P([cx + 7.2, 9.4, 0]);
      ctx.textAlign = 'left'; ctx.fillText('SR', Math.max(4, l1[0]), Math.min(H - 4, l1[1] + 12));
      ctx.textAlign = 'right'; ctx.fillText('SL', Math.min(W - 4, l2[0]), Math.min(H - 4, l2[1] + 12));
      ctx.textAlign = 'center'; ctx.fillText('audience', W / 2, H - 5);
    }

    function drawBeam(fx, aim, c, b) {
      var from = [fx.x, fx.y, fx.z];
      if (fx.role === 'ust') {
        // Two heads per beam fixture: a narrow pair.
        [-0.9, 0.9].forEach(function (dx) {
          beam(from, [aim[0] + dx * (aim[2] > 5 ? 1 : 0.15), aim[1], aim[2]], 0.18, c, 0.5 * b);
        });
        glow(from, 6, c, 0.9 * b);
        return;
      }
      var r = fx.role === 'atwash' ? 1.5 : fx.role === 'atspot' ? 0.55 : 0.45;
      beam(from, aim, r, c, (fx.role === 'atwash' ? 0.2 : 0.38) * b);
      pool([aim[0], aim[1], Math.max(0, aim[2] - 1.1)], r * 1.1, c, (fx.role === 'atwash' ? 0.22 : 0.45) * b);
      glow(from, 6, c, 0.9 * b);
    }

    return {
      attach: attach,
      draw: function () { if (holder && (!camera || canvas.width !== Math.round(holder.getBoundingClientRect().width * dpr))) resize(); draw(); },
      reframe: function () { resize(); draw(); }
    };
  })();

  /* ------------------------------------------------------ command line -- */

  function tokens(line) {
    var out = [], re = /"([^"]*)"|(\S+)/g, m;
    while ((m = re.exec(line))) out.push(m[1] != null ? m[1] : m[2]);
    return out;
  }
  function findPalette(type, word) {
    var w = String(word).toLowerCase();
    var list = PAL[type] || [];
    var p = list.find(function (q) { return q.name.toLowerCase() === w; });
    if (!p && type === 'Dimmer' && /^\d+$/.test(w)) p = list.find(function (q) { return q.name === w + '%'; }) ||
      list.find(function (q) { return Math.round(q.level * 100) === +w; });
    if (!p && type === 'Position') p = list.find(function (q) { return q.name.toLowerCase() === 'aim ' + w; });
    return p;
  }
  function runCommand(line) {
    var t = tokens(line.trim());
    if (!t.length) return null;
    var w0 = t[0].toLowerCase();
    if (w0 === 'help') return 'production "name" · list "name" · cue + "name" · <group> colour <c> dimmer <d> aim <target> · undo';
    if (w0 === 'undo') { undo(); return '✓ undo'; }
    if (w0 === 'redo') { redo(); return '✓ redo'; }
    if (w0 === 'production') { if (!t[1]) return '✗ production "name"'; addProduction(t[1]); return '✓ ' + line; }
    if (w0 === 'list') { if (!t[1]) return '✗ list "name"'; addList(t[1]); return '✓ ' + line; }
    if (w0 === 'cue') {
      if (t[1] !== '+') return '✗ cue + "name"';
      var made = addCue(t[2] || '', false);
      return made ? '✓ ' + line : '✗ no cue list selected';
    }
    var g = groupOf(t[0]);
    if (!g) return '✗ no group, palette or command called ' + t[0];
    var r = resolve(ui.sel);
    if (!r.cue) return '✗ select a cue first (or cue + "name")';
    var set = {};
    for (var i = 1; i < t.length; i += 2) {
      var key = t[i].toLowerCase(), val = t[i + 1];
      if (val == null) return '✗ ' + t[i] + ' needs a value';
      var type = key === 'colour' || key === 'color' ? 'Colour' : key === 'dimmer' || key === 'level' ? 'Dimmer' :
        key === 'aim' ? 'Position' : key === 'shutter' ? 'Shutter' : null;
      if (!type) return '✗ ' + t[i] + '? Use colour, dimmer, aim or shutter';
      var p = findPalette(type, val);
      if (!p) return '✗ no ' + type.toLowerCase() + ' palette ' + val;
      set[ATTR_OF[type]] = p.name;
    }
    var existing = r.cue.layers.find(function (y) { return y.group.toLowerCase() === g.name.toLowerCase(); });
    commit(function () {
      var y = existing;
      if (!y) { y = { id: newId(), group: g.name, colour: null, level: null, aim: null, shutter: null }; r.cue.layers.push(y); }
      Object.keys(set).forEach(function (k) { y[k] = set[k]; });
      ui.sel = { k: 'cue', p: r.prod.id, l: r.list.id, c: r.cue.id };
    }, changedFrom(r.prod, r.list, r.cueIdx));
    return '✓ ' + line;
  }

  /* ------------------------------------------------------------ dialogs -- */

  function dialog(html, onReady) {
    var scrim = document.createElement('div');
    scrim.className = 'scrim';
    scrim.innerHTML = '<div class="dlg" role="dialog" aria-modal="true">' + html + '</div>';
    document.body.appendChild(scrim);
    function close() { scrim.remove(); }
    scrim.addEventListener('click', function (e) { if (e.target === scrim) close(); });
    scrim.addEventListener('keydown', function (e) { if (e.key === 'Escape') close(); });
    onReady(scrim.querySelector('.dlg'), close);
  }
  function askName(title, label, value, ok) {
    dialog('<h3>' + esc(title) + '</h3><label for="dlgname">' + esc(label) + '</label>' +
      '<input id="dlgname" type="text" autocomplete="off" value="' + esc(value || '') + '">' +
      '<div class="btns"><button type="button" class="btn" data-x="cancel">Cancel</button><button type="button" class="btn primary" data-x="ok">OK</button></div>',
    function (dlg, close) {
      var inp = dlg.querySelector('input');
      inp.focus(); inp.select();
      function done() { var v = inp.value.trim(); if (!v) { inp.focus(); return; } close(); ok(v); }
      dlg.querySelector('[data-x=ok]').onclick = done;
      dlg.querySelector('[data-x=cancel]').onclick = close;
      inp.addEventListener('keydown', function (e) { if (e.key === 'Enter') done(); });
    });
  }
  function workFile(name) {
    return JSON.stringify({ kind: FILE_KIND, version: 1, show: SHOW.show, student: name || '', saved: new Date().toISOString(), work: work }, null, 1);
  }
  function saveDialog() {
    dialog('<h3>Save to file</h3><p>Saves your practice show as a small file you can hand in or open later. (A practice file, not a .qlcc: the real app can’t open it.)</p>' +
      '<label for="dlgname">Your name</label><input id="dlgname" type="text" autocomplete="name" placeholder="Jordan">' +
      '<div class="btns"><button type="button" class="btn" data-x="copy">Copy as text</button>' +
      (window.PRACTICE_NO_DOWNLOAD ? '' : '<button type="button" class="btn primary" data-x="dl">Download</button>') + '</div>' +
      '<p id="dlgmsg"></p><textarea id="dlgtext" readonly hidden aria-label="Your work as text"></textarea>' +
      '<div class="btns"><button type="button" class="btn" data-x="close">Close</button></div>',
    function (dlg, close) {
      var inp = dlg.querySelector('input'), msg = dlg.querySelector('#dlgmsg'), ta = dlg.querySelector('textarea');
      inp.focus();
      function fname() { var n = inp.value.trim().replace(/[^A-Za-z0-9_-]+/g, '-'); return 'Lesson01-' + (n || 'practice') + '.json'; }
      var dl = dlg.querySelector('[data-x=dl]');
      if (dl) dl.onclick = function () {
        try {
          var blob = new Blob([workFile(inp.value.trim())], { type: 'application/json' });
          var a = document.createElement('a');
          a.href = URL.createObjectURL(blob); a.download = fname();
          document.body.appendChild(a); a.click(); a.remove();
          setTimeout(function () { URL.revokeObjectURL(a.href); }, 5000);
          msg.textContent = 'Downloading ' + fname() + '. If nothing arrives, use Copy as text and paste it into a document.';
          ui.saved = true; persist(); renderSteps();
        } catch (e) { msg.textContent = 'This browser blocked the download: use Copy as text.'; }
      };
      dlg.querySelector('[data-x=copy]').onclick = function () {
        ta.hidden = false; ta.value = workFile(inp.value.trim());
        ta.focus(); ta.select();
        var okMsg = 'Copied. Paste it into a document or an email to yourself; Open file → Paste brings it back.';
        try {
          navigator.clipboard.writeText(ta.value).then(function () { msg.textContent = okMsg; },
            function () { msg.textContent = 'Selected: press Ctrl+C (⌘C on a Mac) to copy.'; });
        } catch (e) { msg.textContent = 'Selected: press Ctrl+C (⌘C on a Mac) to copy.'; }
        ui.saved = true; persist(); renderSteps();
      };
      dlg.querySelector('[data-x=close]').onclick = close;
    });
  }
  function openDialog() {
    dialog('<h3>Open a saved file</h3><p>Pick a file saved from this page, or paste its text. It replaces what’s here now (Undo brings this back).</p>' +
      '<input id="dlgfile" type="file" accept=".json,application/json,text/plain">' +
      '<textarea id="dlgpaste" placeholder="…or paste the text here" aria-label="Paste saved text"></textarea>' +
      '<p id="dlgmsg"></p>' +
      '<div class="btns"><button type="button" class="btn" data-x="cancel">Cancel</button><button type="button" class="btn primary" data-x="ok">Open</button></div>',
    function (dlg, close) {
      var msg = dlg.querySelector('#dlgmsg'), ta = dlg.querySelector('textarea');
      dlg.querySelector('input[type=file]').onchange = function (e) {
        var f = e.target.files && e.target.files[0];
        if (!f) return;
        var rd = new FileReader();
        rd.onload = function () { ta.value = String(rd.result || ''); msg.textContent = f.name + ' read: click Open.'; };
        rd.onerror = function () { msg.textContent = 'Couldn’t read that file.'; };
        rd.readAsText(f);
      };
      dlg.querySelector('[data-x=cancel]').onclick = close;
      dlg.querySelector('[data-x=ok]').onclick = function () {
        var data;
        try { data = JSON.parse(ta.value); } catch (e) { msg.textContent = 'That isn’t a file saved from this page (it isn’t readable JSON).'; return; }
        var w = data && data.work ? data.work : data;
        if (!validWork(w)) { msg.textContent = 'That file doesn’t hold a practice show from this page.'; return; }
        close();
        commit(function () { work = cleanWork(w); ui.sel = null; runner.pos = -1; runner.next = 0; });
        status('Opened' + (data.student ? ' ' + data.student + '’s' : '') + ' practice show.', true);
      };
    });
  }
  function aboutDialog() {
    dialog('<h3>About this practice version</h3>' +
      '<p>This page copies the Show Tree of <b>qlcconsole</b> closely enough to do Lesson 1.1 on any computer with a browser, Chromebooks included. It is not the real app.</p>' +
      '<p><b>It does:</b> productions, cue lists, cues and layers; colour, level, aim and shutter from the teaching show’s palettes; tracking (grey rows); fade-in times; GO, Back and Stop; a simple stage drawing.</p>' +
      '<p><b>It doesn’t:</b> open or save .qlcc show files, send DMX, or do Block and Cue-only cues, cue looks, effects, sub-cues, reusable layers, the Look Editor or the full command line. The stage is a sketch, not the Rig Overview’s 3D view.</p>' +
      '<p>Your work is kept in this browser on this computer. Use <b>Save to file</b> to keep a copy or hand it in.</p>' +
      '<div class="btns"><button type="button" class="btn" data-x="reset">Start over…</button><button type="button" class="btn primary" data-x="close">Close</button></div>',
    function (dlg, close) {
      dlg.querySelector('[data-x=close]').onclick = close;
      dlg.querySelector('[data-x=reset]').onclick = function () {
        close();
        dialog('<h3>Start over?</h3><p>This clears the practice show in this browser. Undo brings it back until you close the page.</p>' +
          '<div class="btns"><button type="button" class="btn" data-x="no">Cancel</button><button type="button" class="btn primary" data-x="yes">Start over</button></div>',
        function (d2, close2) {
          d2.querySelector('[data-x=no]').onclick = close2;
          d2.querySelector('[data-x=yes]').onclick = function () {
            close2();
            commit(function () { work = emptyWork(); ui.sel = null; ui.ran = {}; ui.saved = false; runner.pos = -1; runner.next = 0; });
            status('Started over.', true);
          };
        });
      };
    });
  }

  function loadExample() {
    commit(function () {
      var w = emptyWork();
      var pid = w.nextId++, lid = w.nextId++;
      function L(group, colour, level) { return { id: w.nextId++, group: group, colour: colour, level: level, aim: null, shutter: null }; }
      w.productions.push({ id: pid, name: 'Spring Show', lists: [{ id: lid, name: '01-Everybody Scream', cues: [
        { id: w.nextId++, name: 'Pose', fade: 0, layers: [L('TrussC-RGB', 'Blue', 'Full')] },
        { id: w.nextId++, name: 'Verse', fade: 0, layers: [L('StepTops', 'Warm White', '75%')] },
        { id: w.nextId++, name: 'Chorus', fade: 0, layers: [L('Steps', 'Magenta', 'Full')] }
      ] }] });
      w.current = pid;
      work = w;
      ui.sel = { k: 'cue', p: pid, l: lid, c: w.productions[0].lists[0].cues[2].id };
    });
    status('Loaded the finished Lesson 1.1. Undo takes it back.', true);
  }

  /* ------------------------------------------------------------ events -- */

  function setPane(p) {
    ui.pane = p;
    var tv = root.querySelector('.tree-view');
    if (tv) tv.dataset.pane = p;
    root.querySelectorAll('.panes-switch .tab').forEach(function (t) { t.classList.toggle('on', t.dataset.pane === p); });
    stage.draw();
  }
  function select(sel) {
    ui.sel = sel;
    persist();
    render();
  }
  function selFromAttr(s) {
    var a = s.split('|');
    var sel = { k: a[0], p: +a[1] };
    if (a.length > 2) sel.l = +a[2];
    if (a.length > 3) sel.c = +a[3];
    if (a[0] === 'layer') sel.y = +a[4];
    if (a[0] === 'tracked') sel.g = decodeURIComponent(a[4]);
    return sel;
  }

  root.addEventListener('click', function (e) {
    var t = e.target.closest('[data-act],[data-sel],[data-drop],.item');
    if (!t || !root.contains(t)) return;
    // Tap-to-place: a picked Library item goes where you tap.
    if (ui.picked && t.dataset.drop) { dropOn(ui.picked, t.dataset.drop); return; }
    if (t.classList.contains('item')) {
      ui.picked = ui.picked === t.dataset.item ? null : t.dataset.item;
      markPicked();
      if (ui.picked) {
        var g = ui.picked[0] === 'g';
        status(itemLabel(ui.picked) + ' picked: tap ' + (g ? 'a cue or the dashed box' : 'a layer row') + ' to place it (Esc to drop it).', true);
        if (window.matchMedia('(max-width: 760px)').matches && resolve(ui.sel).cue) setPane('page');
      } else status('');
      return;
    }
    var act = t.dataset.act;
    if (!act && t.dataset.sel) { select(selFromAttr(t.dataset.sel)); if (t.classList.contains('row') && window.matchMedia('(max-width: 760px)').matches) setPane('page'); return; }
    switch (act) {
      case 'add-prod': askName('New production', 'Name', '', addProduction); break;
      case 'add-list': askName('New cue list', 'Name (a leading number numbers its cues: 01-… starts at 1.0.0)', '', addList); break;
      case 'add-cue': addCue('', true); break;
      case 'delete': deleteSelected(); break;
      case 'undo': undo(); break;
      case 'redo': redo(); break;
      case 'save': saveDialog(); break;
      case 'open': openDialog(); break;
      case 'about': aboutDialog(); break;
      case 'example': loadExample(); break;
      case 'tab': ui.tab = t.dataset.tab; ui.picked = null; persist(); render(); break;
      case 'pane': setPane(t.dataset.pane); break;
      case 'view':
        ui.view = t.dataset.view; persist();
        root.querySelectorAll('[data-act=view]').forEach(function (b) { b.classList.toggle('on', b.dataset.view === ui.view); });
        stage.reframe(); break;
      case 'make-current': {
        var r0 = resolve(ui.sel);
        commit(function () { work.current = r0.prod.id; runner.pos = -1; runner.next = 0; });
        status(r0.prod.name + ' is current: GO plays it.', true);
        break;
      }
      case 'to-cue': {
        var r1 = resolve(ui.sel);
        if (r1.cue) select({ k: 'cue', p: r1.prod.id, l: r1.list.id, c: r1.cue.id });
        break;
      }
      case 'addgroup': {
        var s = document.getElementById('addgroup');
        if (s && s.value) addLayer(resolve(ui.sel), s.value);
        break;
      }
      case 'setattr': {
        var r2 = resolve(ui.sel), k = selKind(), attr = t.dataset.attr, val = t.dataset.val || null;
        if (k === 'layer') setAttr(r2, r2.layer, attr, val);
        else if (k === 'tracked' && val) setInThisCue(r2, r2.group, attr, val);
        break;
      }
      case 'go': go(); break;
      case 'back': back(); break;
      case 'stop': stop(); break;
      case 'load': loadCue(+t.dataset.idx, false); break;
    }
  });
  root.addEventListener('dblclick', function (e) {
    var row = e.target.closest('tr[data-act=load]');
    if (row) { loadCue(+row.dataset.idx, true); return; }
    var it = e.target.closest('.item');
    if (it) { ui.picked = null; markPicked(); useOnSelection(it.dataset.item); }
  });

  // Names and fades commit on Return or when the box loses focus.
  root.addEventListener('keydown', function (e) {
    var t = e.target;
    if (t.id === 'cmd' && e.key === 'Enter') {
      var line = t.value;
      var msg = runCommand(line);
      if (msg) {
        document.getElementById('cmdmsg').textContent = msg;
        status(msg, true, msg[0] === '✗');
        if (msg[0] !== '✗') t.value = '';
      }
      return;
    }
    if (t.dataset && t.dataset.act === 'rename' && e.key === 'Enter') { renameSelected(t.value); return; }
    if (t.id === 'cue-fade' && e.key === 'Enter') { t.blur(); return; }
    if (t.classList && t.classList.contains('item') && (e.key === 'Enter' || e.key === ' ')) {
      e.preventDefault(); useOnSelection(t.dataset.item); return;
    }
    if (t.dataset && t.dataset.sel && (e.key === 'Enter' || e.key === ' ')) {
      e.preventDefault();
      if (ui.picked && t.dataset.drop) dropOn(ui.picked, t.dataset.drop);
      else select(selFromAttr(t.dataset.sel));
    }
  });
  root.addEventListener('change', function (e) {
    var t = e.target;
    if (t.dataset && t.dataset.act === 'rename') renameSelected(t.value);
    if (t.id === 'cue-fade') {
      var r = resolve(ui.sel), v = Math.max(0, Math.min(60, Number(t.value) || 0));
      if (r.cue && r.cue.fade !== v) commit(function () { r.cue.fade = v; }, cueLabel(r.prod, r.list, r.cueIdx));
    }
  });
  root.addEventListener('input', function (e) {
    if (e.target.id === 'filter') { ui.filter = e.target.value; applyFilter(); }
  });

  // Drag and drop (mouse).
  root.addEventListener('dragstart', function (e) {
    var it = e.target.closest && e.target.closest('.item');
    if (!it) return;
    dragItem = it.dataset.item;
    try { e.dataTransfer.setData('text/plain', itemLabel(dragItem)); e.dataTransfer.effectAllowed = 'copy'; } catch (x) { /* old browser */ }
  });
  root.addEventListener('dragend', function () {
    dragItem = null;
    root.querySelectorAll('.droptarget').forEach(function (el) { el.classList.remove('droptarget'); });
  });
  root.addEventListener('dragover', function (e) {
    if (!dragItem) return;
    var t = e.target.closest('[data-drop]');
    root.querySelectorAll('.droptarget').forEach(function (el) { if (el !== t) el.classList.remove('droptarget'); });
    if (!t) return;
    var plan = planDrop(dragItem, t.dataset.drop);
    if (plan.ok) {
      e.preventDefault();
      try { e.dataTransfer.dropEffect = 'copy'; } catch (x) { /* ignore */ }
      t.classList.add('droptarget');
      status(plan.text);
    } else if (plan.text) status(plan.text, false, true);
  });
  root.addEventListener('drop', function (e) {
    var t = e.target.closest('[data-drop]');
    if (!t || !dragItem) return;
    e.preventDefault();
    var it = dragItem;
    dragItem = null;
    t.classList.remove('droptarget');
    dropOn(it, t.dataset.drop);
  });

  document.addEventListener('keydown', function (e) {
    var typing = /^(INPUT|TEXTAREA|SELECT)$/.test((e.target && e.target.tagName) || '');
    if (e.key === 'Escape' && ui.picked) { ui.picked = null; markPicked(); status(''); return; }
    if ((e.ctrlKey || e.metaKey) && !e.altKey && (e.key === 'z' || e.key === 'Z') && !typing) {
      e.preventDefault();
      if (e.shiftKey) redo(); else undo();
      return;
    }
    if (ui.tab === 'run' && !typing && !document.querySelector('.scrim')) {
      if (e.key === ' ' || e.key === 'Spacebar') { e.preventDefault(); go(); }
      else if (e.key === 'Backspace') { e.preventDefault(); back(); }
    }
  });

  /* ------------------------------------------------------------- start -- */

  load();
  fixSelection();
  shell();
  render();
  status(work.productions.length ? 'Welcome back: your practice show is as you left it.' : 'Practice version: start with + Production.');
})();
