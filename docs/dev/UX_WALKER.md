# The UX walker

A screen walker that uses qlcconsole the way a person testing it does, on a
real show, and checks what it sees against the rule book
([UX_RULES.md](UX_RULES.md), [ux_rules.json](ux_rules.json)). Offscreen,
with settings and a HOME of its own: it never touches the real screen or the
person's settings. **Report-only and opt-in** -- it is not in the gate.

```sh
make ux-walk                                   # the default shows, Feet and Millimetres
make ux-walk SHOW=path/show.qlcc UNITS=feet OUT=/tmp/walk
make ux-walk HOSTS=brm,dink                    # this Mac and two pool hosts
make ux-walk HOSTS=auto NOLOCAL=1 REV=<sha>    # only the pool, a committed SHA
make ux-walk FLOWS=1                           # only the core flows (~40 s)
open uxwalk-out/report.html
```

The default shows are `test-workspaces/DanceHysteria-0.6.qlcc` and
`test-workspaces/Office.qlcc` when they are there (they embed their fixture
definitions), else the finished course show. The last line is
`uxwalk: N screens, M findings (K rules)`; the exit status is always 0.

## What it walks

`tools/uxwalk/uxwalk.cpp` (built only with `-DQLC_UXWALK=ON`; `make ux-walk`
configures `build-uxwalk/`) loads the show with Settings ▸ Units set first,
then walks one or more groups:

| group | what |
|---|---|
| `tabs` | every main tab, Everything view, at 1280x800 and 1024x640 |
| `menus` | every menu of the menu bar and every submenu (items, enabled, shortcuts, tooltips) |
| `actions` | every menu item ending in "…" (the canon's mark of a window to finish in), except the deny-list (`kDenyStarts`: New, Open, Save, Quit, Delete, Remove, Clear, Blackout, Operate, Import, Export, ...), triggered; each window it opens is recorded, then Cancelled. An item that changed the show gets the show reloaded |
| `screens` | layoutsanity_test's screen registry (~90 screens) at 1280x800 and at its minimum; `uxwalk.cpp` includes `layoutsanity_test.cpp`, so a screen added there is walked here with no change |
| `context` | right-click menus of: a fixture and a group (Fixtures), a cue, a layer, a hit and a Library group (Show Tree), a look and an effect (Build), a fixture, a target and the canvas (Lighting Studio plot); submenus read as they would open |
| `contextdialogs` | every "…" item of those menus, picked as Return would, and the window it opens recorded (Fixture Properties, Edit Truss, Edit Target, Patch, Output Rules, ...) |
| `showtree` | the Show Tree's page for a cue, a hit, and a layer with an effect, an aim and a colour |
| `pickers` | the effect picker opened from an effect look on a mover group, a par group and a WLED group of the show, with every effect it offers |
| `cmdline` | Build's command line with a typo line and a plain-words line (no AI provider configured), and the message it gives |
| `flows` | the core flows of [ux_flows.json](ux_flows.json), step by step (below) |

A step the show has nothing for (no hit, no WLED group) is a note, not a
finding: the report's summary counts them as "steps not in these shows".

Each state is written as `OUT/screens/<show>-<units>__<group>__<name>.png`
and `.json`: for a window, every visible widget's class, objectName, path,
geometry, text, tooltip, statusTip, placeholder, list/tree items on screen
(with their tooltips and whether each is cut short), combo entries, headers,
enabled state, whether its text is wider than it (`elided`, `need`), Title
Case problems of a button, an empty list and the label beside it, the
window's helpTopic and whether it is a child of the main window, plus
layoutsanity_test's layout findings; for a menu, every item.

## What it checks

`tools/uxwalk/report.py` reads the records and runs the checks. Findings are
grouped: one entry per distinct problem, with every screen it was seen on.

From the rule book (ids as in `ux_rules.json`):

- every rule with a `patterns.forbid_regex` over `ui_text`, applied to the
  app's own words (labels, buttons, menu items, tooltips, titles, headers,
  messages -- not the show's names in lists), as written there;
- **UX-01** lengths follow Settings ▸ Units: no metric length while Feet,
  no imperial while Millimetres (units_test's patterns, tuned; a text that
  shows both kinds is about units and is skipped);
- **UX-07 / UX-08** Title Case for menu items and buttons
  (`ContextMenu::checkTitleCase`, menuwords_test's reading; submenus that
  list the show's own groups, layers and folders are left alone);
- **UX-09** "..." for "…", and an item ending in "…" that opened no window;
- **UX-20** the effect picker offers no WLED effect, and nothing that doesn't
  fit, for a group with no WLED device;
- **UX-21** a plain-words line isn't answered as a typo; the reply fits;
- **UX-23** an empty list or drop-down with nothing beside it saying why;
- **UX-24** a dialog with no helpTopic (F1 has nothing to open);
- **UX-27** a greyed button or menu item with no tooltip saying why (Undo,
  Copy, Paste and the like explain themselves);
- **UX-34** overlap, clipped, off-window, zero-size (layoutsanity's checker);
- **UX-36** text wider than its label, button, combo or spin box; a list
  row cut short at the laptop size and up (not in the Virtual Console,
  which is the person's own drawing);
- **UX-38** a window opened from a menu that is not a child of the main window.

Its own: **W-DUP** the same item twice in one menu, **W-MOJIBAKE** UTF-8
read as Latin-1, **W-NOMENU** a right-click that opened nothing,
**W-CRASH** the app crashed on a step (the stack is in the finding),
**W-CHANGED** a step that should only ask left the show modified.

The rule book's walker rules not checked yet are listed in
`findings.json` (`rulesNotChecked`): UX-06, UX-22, UX-26, UX-49.

## Flows

The `flows` group (`tools/uxwalk/uxwalkflows.inc`) drives the core tasks of
[UX_RULES.md](UX_RULES.md#flows) as a person does them, once per walk, on
the show `ux_flows.json` names (the course's `lesson-3-8-start.qlcc`), in
Feet; each flow starts from the show as saved, in Design. A step is one of:

| `do` | what the walker does | reachable when |
|---|---|---|
| `tab` | picks the main tab | the tab is in the tab bar |
| `node` | selects the Show Tree's production, list, cue, hit or section row | the row is in the tree, the tree on screen |
| `fold` | folds a section row | it has cues under it |
| `plot` | selects a fixture of that model on the plot | the plot is on screen |
| `click` | clicks the button by objectName | shown and enabled |
| `choose` | picks a drop-down entry (regex) | shown, the entry there and enabled |
| `context` | right-clicks a Show Tree row or `plot:<model>` and picks an item | the item is in its menu |
| `action` | triggers a menu-bar action by objectName | in a menu, enabled |
| `menu` | picks an item of a tool button's menu | the button shown, the item enabled |
| `key` | presses a key, to `to` (a panel) or the focus | `to` is shown |
| `code` | what JSON can't say: load a scene into the Workbench, open a saved show with a newer autosave beside it | |
| `{}` | nothing; only the step's `check` | |

A window the step opens gets the step's `dialog` sub-steps (the same
kinds), then `close`: `accept`, `reject` or a button's objectName. A
`find` step does nothing: it asks whether a control (`widget`, visible
`text`, or a `context` item) is on the screen the person is on.

**The dead-end check.** Before a step acts, its control must be on the
screen the step before left. When it isn't, the walker reads what is on
that screen (labels, buttons, titles, the rows of its trees and lists; not
tooltips) for the step's `pointer` words: found, the step is `pointed`;
not found, it is a **dead end** (UX-61); a control that is nowhere in the
app is `missing`. After acting, the step's `check` (`visible`, `hidden`,
`text`, `widgetText`, `comboHas`, `rows`, `folded`, `running`, `window`,
`cues`, `menuItem`) says whether it did what the step says. The first
failure stops the flow (later steps: `notreached`), except a `find` step,
which is a finding and the flow goes on. A `manual` step is not driven;
its reason is in the report.

`report.py` turns flow results into findings: a dead end is UX-61; a step
`missing` or not as expected is the step's own `rule` (UX-62, UX-64,
UX-65) or **W-FLOW**; clicks over the flow's budget are UX-66. The report
opens with the flow table (result, clicks against budget, first failure),
then each flow's steps with their screenshots; `findings.json` has them
under `flows`; the run ends with a `uxwalk-flow:` line per flow.

On main at 415eafa95 (2026-11-07), about 40 s: F-05 GO, F-06 Quick Focus
and F-08 Units pass; F-07 (follow spot) is manual; F-01 (sections from
the cue list), F-02 (a key in Design), F-03 (Auto's number), F-04 (record
from the Workbench), F-09 (Sprinkle loops) and F-10 (autosave offered)
fail. Each fail is a 2026-11-07 catch or a fix still on its branch: with
feat/042-key-triggers, fix/042-autosave-recovery and feat/042-wand-spacing
merged on a trial branch, F-02, F-03 (Turns read "Auto (0.5)") and F-10
pass, so those checks see the fix, not only its absence.

Adding a flow: write it in both UX_RULES.md (Flows) and ux_flows.json
(check_rules.py keeps ids, titles, budgets and step counts in step); give
each step the objectName of its control (`grep setObjectName ui/src`) and a
`check` that fails on the wrong result. A step that can't be driven is
`manual` with the reason.

## Sharding: fast on the pool

Every (show x units) pair is independent, and inside a pair the groups
split (`SHARDS` in `tools/uxwalk/run.py`): tabs in 2, the small groups
together, actions in 3, contextdialogs in 3, screens in 4 -- round-robin by
`--shard I/N`. Each shard is one `uxwalk` process with its own settings,
HOME and TMPDIR. The driver puts every shard of every pair on one queue:

- **this Mac** runs `--jobs` of them at once (default half the performance
  cores), under `nice`;
- **each host of `--hosts`** (names from `platforms/macos/gate-hosts.conf`
  or `$GATE_HOSTS`; `auto` = all of them) first takes the gate pool's lock
  (`~/gates/.gatelock` via `gate-lock.sh`, so it never shares a host with a
  gate, and holds it only for the walk), syncs the commit's tree into
  `~/uxwalk/src` by checksum (so the build in `~/uxwalk/build` is
  incremental), builds `uxwalk`, copies the shows, and then takes shards
  from the same queue with half its performance cores; its records come
  back by rsync after each shard.
- **the flows** are one more shard (`--no-flows` leaves them out,
  `--flows-only` runs only them).

A slow host only joins late; nobody waits for it. A crash or hang (900 s)
costs one step, not the shard: `uxwalk` prints `uxwalk-at: N group|name`
before each step and a stack on SIGSEGV/SIGBUS/SIGABRT; the driver writes a
W-CRASH note with the stack and reruns the shard with `--skip N`.

Times (2026-11-06, both default shows in Feet and Millimetres, 52 shards,
~1,110 screens): about 3 minutes on brm alone (10 performance cores, 5
walkers); through the pool from another Mac, brm's first (cold) build took
150 s and the walk 200 s -- a later run of a new commit builds only what
changed. A host still building when the queue runs dry is stopped and its
lock let go: the walk never waits for a slow host.

## Adding a check

In `tools/uxwalk/report.py`:

```python
def check_my_rule(rec, ctx):
    for kind, where, text, rect in texts(rec):      # every text of a record
        if kind == "chrome" and "Stage Look" in text:
            yield Finding("UX-12", text, "the old word", where, rect)

CHECKS = [..., check_my_rule]
```

`rec` is one screen's JSON (see above); `texts(rec)` yields each text with
whether it is the app's own (`chrome`) or the show's (`data`). A rule from
the book uses its id (its title and wording come from `ux_rules.json`); one
of the walker's own gets a `W-` id in `BUILTIN`. Give a `sig=` when the same
problem would otherwise count once per screen it is on. Re-run only the
report on a finished walk: `tools/uxwalk/report.py OUT`.

A rule that only needs a pattern over the UI's words needs no code: give it
`patterns.scope: "ui_text"` and `patterns.forbid_regex` in `ux_rules.json`.

## Adding a step

- A screen: add it to layoutsanity_test's `screens()`; both pick it up.
- A right-click target: a `hit(name, open)` in `walkContext()`; its "…"
  items are walked by `contextdialogs` automatically.
- Something new: a `walkXxx(Walker &, App *)` and its group name in
  `kGroups` and `main()`, then a line in `SHARDS`. Call
  `walk.mine(group, name)` before each step (sharding, `--skip`) and
  `walk.record(widget, group, name)` / `walk.recordMenu(...)` for each state.
