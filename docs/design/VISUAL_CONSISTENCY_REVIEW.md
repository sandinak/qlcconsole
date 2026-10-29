# Visual consistency review (2026-08-18)

A look-and-feel review of qlcconsole — spacing, icon usage, color/theme
compliance, and control-widget style — explicitly separate from
[WORKFLOW_UX_REVIEW.md](WORKFLOW_UX_REVIEW.md), which covered
discoverability/workflow instead. Requested directly: "did you also
evaluate look and feel for consistency thru the implementation?" Method:
four parallel passes over the real UI code (icon resource usage, computed
color contrast against each theme, layout margins/spacing, confirmation/
tooltip/slider/disabled-state patterns), cross-checking the app's own theme
system (`App::applyTheme()`) rather than assuming.

## The one fact that reframes everything below

`App::applyTheme()` does a real, application-wide `QPalette` swap
(`qApp->setPalette(pal)`), and `default.qss` deliberately reads colors via
`palette(...)` so it follows automatically. **This means every hardcoded
hex color anywhere in the UI is a deliberate opt-out of theming, not a
minor style choice** — and the risk is real, not theoretical: two chips
added to the footer *this session* (`m_statusModeChipLabel`'s `#555`/
`#2e7d32`) computed to illegible or marginal contrast against all three
dark themes (VS Code Dark, Red Shift, Blue), while reading fine on the
light default they were eyeballed against. **Already fixed** (see below) —
flagging first because it's the most consequential single finding: dark,
low-glare themes exist specifically for working near a stage/audience
(CLAUDE.md), so a theme silently breaking legibility defeats its purpose.

## Fixed today (2026-08-18)

- **Mode chip (`m_statusModeChipLabel`) theme-safety** — DESIGN state now
  uses `color: palette(text)` instead of hardcoded `#555` (computed ~2.2:1
  against the dark themes, well under WCAG's 3:1 floor even bold).
  OPERATE's green moved from `#2e7d32` (tuned for light backgrounds,
  ~3.25:1 on dark — marginal even bold) to `#43a047`, same "live" identity
  with real margin on both light and dark. Fixed in both places this text
  is set (`slotModeChanged()` and the construction-time seed — they'd
  drifted into two separate literal copies of the same styling).
- **Show Lock vs. Blackout color collision** — Show Lock reused Blackout's
  *exact* stylesheet string (`color: #e60000; font-weight: bold;`),
  making a deliberate, safe "don't let go move" toggle read as the same
  alarm tier as the most hazardous console state. Show Lock now uses
  `#a06000` (amber/caution) — reusing the app's own existing convention
  (`connectionstree.cpp`, `universepatchgrid.cpp` both already use
  `#a06000` for "pending/caution") rather than inventing a new color.

Both builds clean, smoke-tested. **Not visually confirmed** — same caveat
as everything else today: no screenshot tool available in this session,
contrast numbers were computed from the hex values against each theme's
known palette, not rendered and eyeballed.

## 1. Icons

1. **Add/Delete icons double-booked for Expand/Collapse.**
   `fixturemanager.cpp:1209-1219` uses `:/edit_add.png`/`:/edit_remove.png`
   for Add/Delete, then `:1336-1337` reuses those *same* icons for "Expand
   all groups"/"Collapse all groups" a few buttons over in the same
   toolbar — direct pattern-recognition break.
2. **Delete/Remove fragmented across three icon files** with no evident
   rule for which manager gets which: `edit_remove.png` (red minus-bar —
   Fixture Manager, Connections), `editdelete.png` (red X-on-document —
   Function Manager, Virtual Console, Show Manager), `delete.png` (red
   circle-slash — Simple Desk).
3. **The real undo icon (`undo.png`) is used for "reset to default"**
   (`virtualconsole.cpp:456,470,483`), while the actual Undo actions
   (Connections, Show Manager) use `back.png` — which is *also* reused for
   "Previous" navigation elsewhere (Simple Desk), so one icon now carries
   three unrelated meanings while the purpose-built undo glyph sits
   unused for undo.
4. **Properties split across two icon families** — Fixture Manager's
   Properties action uses `configure.png` (wrench), Virtual Console's
   equivalent uses `edit.png` (pencil). Same concept, different family.
5. **Sibling context menus, one iconified, one not** — Programming tab's
   function-tree "New Scene/Chaser/…" menu is fully iconified
   (`programmingmanager.cpp:1848-1855`); the palette-tree's "New Color/
   Beam/…" menu, same interaction pattern a few hundred lines later
   (`:2144-2149`), has zero icons.
6. **Duplicate/Delete text-only inside an otherwise-iconified menu**
   (`programmingmanager.cpp:1824-1829`) — right next to the fully-iconified
   "New …" block, missing an easy reuse of `editcopy.png`/`editdelete.png`
   already defined elsewhere for these exact actions.
7. **`fixturemanager.cpp:1253`** — "New Group..." has no icon, unlike
   sibling create-actions (Programming tab's "New Folder" gets
   `folder.png`).
8. **Toolbar icon sizes vary with no shared constant**: 32×32 (Fixture
   Manager, Connections), 26×26 (Virtual Console), 20×20 (Show timeline's
   bottom toolbar), and Function Manager/Show Manager's main toolbars never
   call `setIconSize` at all — falling back to a platform default that
   will visibly differ from the neighboring 32px toolbars on tab-switch.

**Confirmed good, not a finding**: `QMessageBox` severity usage is
consistent app-wide — destructive confirms uniformly `::question`
Yes/No, genuine errors uniformly `::warning`/`::Critical`, no arbitrary
mixing found anywhere sampled.

## 2. Color & theme (beyond what's fixed above)

1. **Semantic red is four different colors** with no shared constant:
   `#e60000` (Blackout, and Show Lock until today), `#e04030` (rig-not-
   ready), `#c0392b` (power overload, audio-detect failure), `#b00`/
   `#bb0000` (look-editor warning). Not a legibility bug, but undermines
   "red always means the same urgency" — worth consolidating into one
   named constant eventually.
2. **`timecodecalibrationdialog.cpp:412`**, `#2c3e50` ("flat UI midnight
   blue") text while listening — computed ~1.5:1 against any dark theme's
   background, essentially invisible. The *same label*'s idle state
   (`:180/435`) uses `#7f8c8d` (~4.7:1, fine) — this isn't even a
   deliberate "ignore theme" choice, just one state nobody checked.
3. **"Good/fine" state has three inconsistent treatments**: "Saved" is a
   gray checkmark (not green); power-widget "within limits ✓" and
   timecode's "(applied ✓)" use flat green `#27ae60`; OPERATE mode (fixed
   above) is a *different* green with no checkmark. Same underlying
   concept, three visual languages.
4. **Confirmed good**: `#a06000` was already consistently reused for
   "pending/caution" in `connectionstree.cpp`/`universepatchgrid.cpp`
   before today — Show Lock's fix above extends a real existing
   convention rather than inventing one.
5. **Lower priority, not verified**: `consolechannel.cpp`/
   `grandmasterslider.cpp` hardcode slider-groove gradients — likely fine
   since they're self-painted backgrounds rather than palette-dependent
   chrome, but never visually confirmed.

## 3. Spacing & layout

1. **Dialog margins are consistent only by omission** — no `.ui` file in
   the whole repo overrides layout margins/spacing (`addfixture.ui`,
   `createfixturegroup.ui`, etc. all fall back to platform default). Not a
   bug today, but a fragile non-convention: nothing enforces it, and the
   next hand-edited dialog could silently diverge.
2. **Hand-built panels use uncoordinated margin constants**: `lookeditor.cpp`
   mostly uses `6,6,6,6`/spacing 6, but its effect-widget container uses
   `0,4,0,4` a few hundred lines away with no apparent semantic reason;
   `inputoutputpatcheditor.cpp` uses a tighter `2,1,2,1`/spacing 3 (arguably
   fine for an embedded tree-cell, but no shared constant exists anywhere
   to express "this is the tight-embedded scale" vs. "this is the normal
   panel scale").
3. **Toolbar icon sizes** (same finding as icons #8 above, from the layout
   angle): 24/26/32/platform-default/none across five managers — genuinely
   the single most visible cross-app inconsistency found in this whole
   review, since it's visible on every tab-switch.
4. **The GM fader is the only footer chip wrapped in its own layout** —
   every other chip is a bare `QLabel` on the status bar's own default
   spacing; GM (added today) is a `QWidget`+`QHBoxLayout` with hand-picked
   `4px` internal spacing with no defined relationship to the ambient gap
   between the other chips. This is somewhat inherent to GM being a
   compound (label+slider+value) control rather than a single label — not
   straightforwardly fixable without redesigning GM as a custom-painted
   single widget, which is a bigger undertaking than this pass. Flagging
   as "inherent, not casually fixable" rather than leaving it unexamined.
5. **No single section-header convention**: native `QGroupBox` titles,
   manually-bolded plain `QLabel`s (each with its own inline
   `setStyleSheet`, no shared style), and bare `QFormLayout`s with no
   header at all all coexist, sometimes in the same file
   (`lookeditor.cpp`). A further inconsistency found in the same file:
   one header label uses theme-safe `palette(mid)`, a neighbor three lines
   away hardcodes `#999` — the exact same class of bug just fixed in the
   footer today, but not yet touched here.

## 4. Control-widget style

1. **The Programming tab's Save button is the only arm-then-confirm
   control in the app.** Every other destructive action (10+ files
   checked) uses a synchronous `QMessageBox::question` Yes/No modal — a
   real, consistent baseline. Save's "click again within 3s" inline
   button-text-change + `QTimer` auto-disarm is a genuinely different
   interaction pattern found nowhere else. (The arm/confirm *mechanic*
   itself predates today's session — only the button's label was renamed
   today — but it's worth flagging as the one true outlier against an
   otherwise-consistent modal convention.)
2. **Tooltip voice/coverage varies, sometimes within one file** —
   `functionmanager.cpp`'s New Scene/Chaser actions get long explanatory
   sentences, its New Palette action gets a terse imperative, and 8 sibling
   create-actions (Sequence, EFX, Collection, RGB Matrix, Script, Audio,
   Video, Folder) get no `setToolTip` at all — falling back to Qt showing
   the raw mnemonic-decorated action text, a third unrelated voice, all in
   the same `initActions()`.
3. **Sliders inconsistently pair with a numeric readout, even within one
   dialog** — the GM footer fader, LookEditor's Dimmer page, and `VCSlider`
   all show a live value. LookEditor's own Color page (R/G/B/W/A/UV
   sliders, added this session) shows only a static "1 R"-style name label
   with **no numeric value at all** — a real gap in work landed today,
   not a pre-existing issue: the numbered-label pass stopped short of
   giving these sliders the same live-readout treatment their Dimmer-page
   sibling already has.
4. **Button vs. toolbar-action has no visible rule** — Fixture/Function
   Manager use `QToolBar`+`QAction` for Add/Remove/Group; Palette Manager
   and essentially all of the Programming tab use plain `QPushButton` rows
   for the same "manage a list"/"trigger an action" role, with no
   criterion (dialog vs. panel doesn't cleanly predict it).
5. **Disabled-vs-hidden is mostly consistent, one outlier**: Fixture
   Manager, Virtual Console, and Programming Manager all correctly grey-out
   (not hide) contextually-unavailable actions — a real, confirmed-good
   baseline. The outlier: `app.cpp`'s Capture Store/Undo toolbar actions
   both hide *and* disable, vanishing entirely rather than staying
   visible-but-greyed like every other contextual action in the app.

---

## Suggested order of attack

Ranked by (a real regression already shipped vs. pre-existing) and (impact
÷ effort):

1. ~~Mode chip / Show Lock theme-safety~~ — **done above.**
2. **Toolbar icon size unification** (icons #8 / spacing #3) — the single
   most visible inconsistency in the whole review (every tab-switch shows
   it), and mechanically simple: pick one size, add the missing
   `setIconSize` calls to the two managers that never set one.
3. **Color page's missing slider value labels** (control-widgets #3) — a
   genuine loose end from this session's own numbered-slider work, not
   archaeology; small, contained fix in `lookeditor.cpp`.
4. **Delete/Remove icon consolidation** (icons #2) — three icons, one
   concept; pick one and standardize, though this touches several files.
5. **`timecodecalibrationdialog.cpp`'s `#2c3e50`** (color #2) — one-line
   fix, worst legibility offender found, isolated to one file.
6. Everything else — icon family drift (Properties, Undo, Expand/Collapse),
   semantic-red consolidation, section-header convention, dialog margin
   convention, the Save button's one-off confirm pattern, tooltip voice —
   lower urgency, broader surface area, worth a dedicated pass rather than
   bolting on fixes here.
