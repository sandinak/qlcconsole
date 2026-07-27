# Testing Status — manual GUI/rig tests (run on-site)

Checklist of hands-on tests for the work landed on `programmer-mode` this session.
Author is remote and can fix issues but **cannot test until back on-site**, so each
item is a discrete, self-contained test with steps + expected result. Tick the box
and jot anything that misbehaves.

**How to launch:** `build/main/qlcplus -o surfacetesting.qxw`
(never pass a positional workspace arg — it can blank the file). Work against a
scratchpad copy if you're worried: `cp surfacetesting.qxw /tmp/st.qxw` then `-o /tmp/st.qxw`.

Legend for what each test needs:
- 🖥️ = 2D monitor / screen only (no rig)
- 💡 = real rig helpful to confirm output
- 🎚️ = needs MIDI Time Code (Logic/QLab) into a patched input universe
- 🎛️ = needs an APC40 / MIDI controller for the mapping test

Unit-tested already (no manual test needed): `LastLookEffect` holder mechanics
(hold/clear/persist/per-fixture-yield) — `engine/test/lastlookeffect`, 6/6 pass.

---

## A. Build workflow — Programming tab

### A1 — Right-click → create fixture group from fixtures 🖥️
1. Programming tab → lower-right **Fixtures & Groups** panel.
2. Select 2–3 **fixtures** (not a group); right-click.
3. Choose **"Create fixture group from N fixtures…"**; name it; accept the grid dialog.
- [ ] The new group appears in the tree.
- [ ] It contains those fixtures (double-click to open — see A5).
- [ ] Dragging it onto a scene works as a dynamic target.
- [ ] Right-clicking a **group** still shows only "Move to folder…" (unchanged).
- [ ] A mixed fixture+group selection shows **both** actions.

### A2 — Open a Show in the Programming canvas 🖥️
1. Func tree (left) → right-click → **New Show** (also confirm the menu lists it).
2. It opens an embedded **timeline** in the center pane (not the old "edit in Functions" text).
- [ ] New/empty show shows **one** clean track lane + a centered hint.
- [ ] **No stack of empty phantom rows, no vertical scrollbar** on an empty show (compact sizing).

### A3 — Build a show by dragging functions onto the timeline 🖥️
1. With a show open (A2), drag a **scene**, a **chaser**, and a **collection** from the left tree onto the timeline.
- [ ] **Each block lands under the cursor** where you dropped it — *not offset to the right* (this was the drop-hotspot fix).
- [ ] Dropping on empty space below the tracks creates a new track for it.
- [ ] **Add Track** toolbar button adds an empty track.
- [ ] Dragging a block sideways moves it in time and the change sticks (re-open shows it moved).
- [ ] Right-click a track → **Delete track** confirms, removes it, keeps the underlying functions.

### A4 — Rows grow to fit 🖥️
1. On a show, add tracks until there are 3–4.
- [ ] The timeline height grows to match the actual track count (still no giant empty area / premature scrollbar).

### A5 — Double-click a group → visualize in the canvas 🖥️
1. Lower-right panel → **double-click a fixture group**.
- [ ] The center pane shows the group's **head-layout grid** (FixtureGroupEditor).
- [ ] Title reads "Fixture group: <name> — N head(s)".
- [ ] Edits to the grid there stick (it's the real editor, not read-only).
- [ ] Selecting a scene afterwards returns to the normal look canvas cleanly.

---

## B. Last-look persistence (the headline engine change)

> Default **ON** in Operate. A stopped/finished **show** should hold its final look
> instead of blacking out, until the next cue takes over.

### B1 — Show stop holds the last look 💡 (rig or 2D monitor)
1. Enter **Operate**. Run a show that ends on a visible look (movers positioned, color, intensity up).
2. Press **Stop** on the show (or let it run to the end of the timeline).
- [ ] The last look **stays on the rig** — no blackout.
- [ ] Intensity holds (not just color/position); movers stay where they were.
- [ ] 2D monitor shows the same held look.

### B2 — Next cue takes over (per-fixture yield) 💡
1. From the held state (B1), fire a **new cue/scene** that covers **all** the same fixtures.
- [ ] The new look fully replaces the held one.
2. Reset to a held state, then fire a **partial** cue (e.g. a scene touching only *some* held fixtures — house lights on 3 of them).
- [ ] Only those fixtures follow the new cue; **the rest keep holding the last look** (this is per-fixture yield, not whole-look wipe).

### B3 — Clear + toggle (main toolbar) 💡
1. Held state active (B1). On the **main toolbar** find **Clear Held Last Look** (✕-style icon).
- [ ] Clicking it drops the hold → those channels go dark.
2. Toggle **Hold Last Look** (star icon) **OFF**, then stop a show.
- [ ] With it off, a stopped show blacks out as before (feature disabled).
- [ ] Toggling off while a look is held also clears it immediately.

### B4 — Mode / lifecycle safety 💡
1. Hold a look (B1), then switch to **Design** mode.
- [ ] The hold clears (you're building again; rig not stuck on the old look).
2. Hold a look, then **open a different workspace / New**.
- [ ] The hold clears (no leftover output).
3. Confirm holding is **Operate-only**: stopping a show in **Design** does not install a hold.

### B5 — Blackout interaction 💡
1. Hold a look (B1). Toggle **Blackout** on.
- [ ] Rig goes dark. Toggle Blackout off → the held look returns.

### B6 — MIDI-mapped Clear (APC40) 🎛️
1. Virtual Console → add a **VC Button** → properties → action **"Timeline: Clear held last look"**.
2. Map an APC40 pad to it (button's input source).
3. In Operate, hold a look (B1), press the pad.
- [ ] The held look clears from the controller.
- [ ] The action **round-trips**: save, reload, re-open the button properties → action still "Clear held last look".

---

## C. Mixed timecode show (integration, needs playback rig) 🎚️

> Only relevant once you have Logic/QLab sending MTC. Confirms the song↔manual-section
> handoff and that a **pause** (not stop) holds.

### C1 — MTC pause holds the look 🎚️💡
1. Operate, Follow-MTC armed, a show chasing timecode mid-song.
2. **Stop the transport** (MTC quarter-frames stop) — the "between songs" pause.
- [ ] The show **freezes** and the current look **holds** (no sag, no blackout).
- [ ] Rolling the transport again resumes chasing from the new position.
- [ ] Watch the first frame after pause for any intensity dip — there should be none.

### C2 — Manual section during a pause 💡🎛️
1. During a held pause (C1), fire a manual **VC cue list** (GO / APC40) for the spoken/break section.
- [ ] It takes over its fixtures; the rest stay held.
2. Roll the next song's timecode.
- [ ] The show relocates (seekTo) and chases the new song.

---

## D. Finished timeline editor (Programming tab) 🖥️

### D1 — No auto-play on open 🖥️
1. Programming tab → open a Show.
- [ ] It does **not** start playing on open (previously auto-played).
- [ ] The timeline toolbar shows **Play/Pause**, **Stop**, **Add Track**, **Delete**.

### D2 — Transport 💡
1. Open a show with content. Press **Play**.
- [ ] The show runs; the playhead cursor moves; the button shows a pause icon.
2. Press **Play** again → pauses (cursor freezes). Again → resumes.
3. Press **Stop** → stops and the cursor rewinds to 0.
4. Switch to another function / leave the tab while playing.
- [ ] The show **stops** (doesn't keep playing behind an unseen tab).

### D3 — Clip + track editing 🖥️
1. Select a clip → press **Delete** (or the toolbar trash) → it's removed; the function is kept.
2. Select 2+ clips (Shift/Ctrl-click) → Delete → confirm dialog → all removed.
3. **Double-click a track header** → rename dialog → name updates.
4. Drag a track header up/down (or right-click → move) → order changes and sticks.
5. Right-click a track header → **colour** → pick → the track recolours.
6. Right-click an empty timeline slot → **Add here** → pick a function → it lands there.
- [ ] All of the above persist across save/reload.

### D4 — Section markers 🖥️
1. Right-click the **time ruler / marker lane** → **Add marker here…** → label it.
- [ ] A coloured region band appears with the label.
2. Right-click a marker → **Rename**, **Change colour**, **Delete** — each works.
3. Drag a marker to move it.
- [ ] Markers round-trip through save/reload.

## E. Timecode↔manual seam (marker ⇄ cue list)

### E1 — Link a manual cue list to a section 🖥️
1. Have a **Chaser** built to cover a spoken/break section (your manual GO stack).
2. In the timeline, right-click a section marker → **Link manual cue list…** → pick that chaser.
- [ ] The marker's label now shows a **⏵ link glyph** in the lane.
3. Save, reload.
- [ ] The link persists (check the `.qxw`: `<Marker … CueList="<id>">`).
4. Rename / recolour / move that marker.
- [ ] The link **survives** all three (not lost).

### E2 — Operator sees the next manual GO at a freeze 🎚️💡 (or fake it in Operate)
1. Add a **VC Show Control** widget bound to the show (Virtual Console).
2. Run the show so the playhead sits inside a **linked** section (roll MTC into it, or
   in Operate scrub the show there).
- [ ] The Show Control shows an **amber "Manual: <cue list name> · next GO: <cue>"** line.
- [ ] It disappears when the playhead leaves that section or the show stops.
3. GO the linked chaser (APC40 / its VC cue list).
- [ ] The "next GO" text advances to the following cue.

## F. Batch quick-wins

### F1 — Palette Delete 🖥️
1. Programming tab → palette tree → right-click a palette → **Delete**.
- [ ] Confirms; if scenes use it, the prompt says how many.
- [ ] The palette is gone and any scenes that used it lose that look (no dangling ref).
2. Select several palettes → right-click → **Delete N palettes** → confirm → all removed.

### F2 — Group rename / delete in the panel 🖥️
1. Lower-right Fixtures & Groups → right-click a **group** → **Rename group…** → new name sticks.
2. Right-click group(s) → **Delete group(s)** → confirm → group removed, **fixtures kept**.

### F3 — Live-DMX snapshot 💡 (needs live output)
1. Get a look onto the rig (external console, or run a preview/scene so fixtures output).
2. Programming tab → open/select the **scene** you want to capture into.
3. Click **Snapshot** (toolbar).
- [ ] Prompt reports N fixtures / M channel values; accept.
- [ ] The scene now holds those values as static (visible in the canvas console).
- [ ] Dark fixtures (all-zero) are skipped; only outputting fixtures captured.
- [ ] With no scene open, it tells you to open one first.

### F4 — Freeze chip timing 🎚️
1. Under MTC follow, stop the transport mid-song.
- [ ] The footer "holding" (amber) chip flips promptly (~when the rig freezes), not
      ~half a second late.
- [ ] Steady playback never flickers to "holding".

## G. Suspend-hold, arming, canvas undo

### G1 — Suspend holds the look 💡 (Operate, a running show)
1. Run a show in Operate on a visible look. Click **Exit Timeline Control** (main toolbar).
- [ ] The look **holds** on the rig (no blackout) while the chip goes amber "SUSPENDED".
- [ ] Bring up a VC look on some of those fixtures → they follow the VC; the rest keep holding.
2. Click **Resume Timeline Control**.
- [ ] The show reclaims its fixtures from the current position (no rewind).
- [ ] With **Hold Last Look** toggled OFF, Suspend reverts to the old behaviour (releases).

### G2 — Auto-arm the manual cue list at a freeze 🎚️💡
1. Link a Chaser to a section marker (§E1). Run the show under MTC into that section, then **stop the transport** (freeze).
- [ ] That chaser **starts automatically** — its first cue lights up.
- [ ] A mapped **GO / APC step** (ChaserStepNext) advances *that* chaser (it's the current one).
- [ ] `VCShowControl` shows green **"▶ ARMED — <name> · next GO: <cue>"**.
- [ ] Scrubbing out and back doesn't restart it if it's already running.

### G3 — Footer run-of-show readout 🎚️
1. Run a show under MTC across sections.
- [ ] The footer MTC chip appends **· <section name>** as the playhead crosses sections.
- [ ] In a linked section it also shows **· manual: <name>** (or **▶ manual armed: <name>**).
- [ ] Visible from any tab (VC, Functions, Programming), not just Show Manager.

### G4 — General canvas Undo (Ctrl+Z) 🖥️
1. Open a scene in the Programming canvas. Make a series of edits: drag a palette (look),
   add a group/fixture target, edit a channel, **Snapshot**, change a per-look fade.
2. Press **Ctrl+Z** repeatedly.
- [ ] Each press reverts the **last** edit, in order (looks/targets/values/fades all covered).
- [ ] The live preview + power footer update after each undo.
- [ ] Renaming the scene in the tree, then editing + undoing, does **not** revert the name.
- [ ] Switching to another scene starts a fresh history (no cross-scene undo).
- [ ] With no scene-edit history, Ctrl+Z still performs the old bundle-stamp undo.

## H. Frozen track-header column 🖥️
1. Open a show with a few tracks and content wide enough to scroll horizontally
   (embedded Programming timeline OR the Show Manager tab — both).
2. Scroll the timeline horizontally.
- [ ] The **track-header column** (names/mute/solo) stays fixed at the left; only the
      clips/ruler scroll under it.
- [ ] The **zoom slider** (top-left corner) and the **vertical divider** stay put too.
- [ ] Clips scrolling past the left go **behind** the header column (not over it).
- [ ] Section-marker bands/lines stop at the header's right edge (don't bleed into it).
- [ ] Vertical scroll still moves the track rows normally (column isn't frozen vertically).
- [ ] Un-scrolled (leftmost) view looks exactly as before (no regression).

## I. Per-track flags (Show timeline)

### I1 — Intensity submaster 💡 (live)
1. Run a show (Operate) with a lit track. On the track header, **drag the intensity bar**.
- [ ] The track's output scales live (movers/wash dim); the bar shows % and turns amber below 100%.
- [ ] Set 60%, save, reload → the bar restores at 60% (XML `Intensity=`).
- [ ] Dragging one track's bar doesn't affect other tracks.

### I2 — Solo / solo-safe / mute 💡 (LIVE mid-run)
1. Right-click a track header → **Solo-safe (mute-exempt)** on a house/work-light track.
2. With the show **running**, hit **M** (mute) on a track.
- [ ] That track's output stops **immediately** (no restart); other tracks keep playing unchanged.
- [ ] Un-mute mid-cue → the track's cue resumes at the correct position (not from 0).
3. With the show running, **Solo** a track (S button).
- [ ] Only the soloed track (and any solo-safe) output, live; un-solo restores the rest.
- [ ] Solo then **save without un-soloing** → other tracks are NOT saved as muted (non-destructive).
- [ ] Two tracks soloed at once → both output (additive).

### I3 — Hold-last 💡
1. Right-click a track → **Hold last look**. Give it a cue that ends before the show does.
2. Run the show past that cue's end (Operate).
- [ ] The track's look **stays on the rig** after its cue ends (doesn't go dark).
- [ ] The track's next cue takes over its fixtures cleanly.
- [ ] Other (non-hold-last) tracks release to black as normal when their cues end.

### I4 — Priority / role 💡
1. Two tracks on the same fixtures: a base wash (set **Background**) and an accent (set **Override**).
2. Run both.
- [ ] The Override track wins those channels (LTP); the Background track yields.
- [ ] A VC action beats a Background track; Normal is last-writer-wins as before.
- [ ] Priority persists across save/reload (XML `Priority=`).

## J. Effect engine: sharing / authoring / parity scripts 🖥️💡

### J1 — EFX-parity scripts
1. New palette → Effect → open the picker.
- [ ] **Lissajous** and **Diamond** appear (Position category). Apply to movers → beams
      trace the curve/diamond; params (freq/phase/radius/spread) behave.

### J2 — Export / Import an effect 🖥️
1. Build an Effect look (pick a Generator, tweak params) → **Export…** → save a `.qxfx`.
- [ ] The file is written; open it in a text editor → JSON has params + `scriptSource`.
2. On another machine (or after deleting the user preset), **Import…** that file.
- [ ] The effect appears in the picker; if you didn't have the script, it's installed
      to the user scripts dir and works.

### J3 — Script authoring + editor preference 🖥️
1. **App Settings → Effects → "Effect script editor"** → set **In-app editor**.
2. Effect page → **New script…** → name it.
- [ ] A template opens in the in-app editor; edit + **Save** → it rescans and the
      new script shows in the picker.
3. Set the preference to **External editor**, click **Edit script…** on a Generator.
- [ ] It opens in the OS default `.js` editor instead.
- [ ] The preference persists across restarts.

## §TC — Timecode + Show-length batch (2026-07-27)

### §TC.1 Sync-health readout 🎚️
1. Roll MTC from Logic into a patched input; click the footer **MTC chip**.
- [ ] Top of the menu shows **Sync: ✓ healthy** and a line `1.00× · jitter N ms · ~M ms/update`.
- [ ] Stop Logic → verdict flips to **held**; a jittery/RTP feed → **~ usable** or **⚠ unstable**.

### §TC.2 Manual tap calibration 🎚️
1. MTC chip menu → **Calibrate offset…**. Roll the show; pick a reference (Timeline
   start, or a section marker). Tap **space** on the beat a few times over a couple rolls.
- [ ] Each tap appends `#N manual … → offset …`; **mean + spread** update; feed health shows live.
- [ ] **Apply offset** writes it (Current show offset updates); **±10 ms** trims; taps against an
      unstable feed are flagged orange.

### §TC.3 Audio click-track auto-tap 🎚️🎧 (needs a click bus → QLC audio input)
1. In Calibrate, set **Click BPM** to the track tempo, press **● Listen (auto-tap)**.
- [ ] The **beat lamp** flashes bright-green on *every* detected click (even with no TC rolling —
      confirms QLC hears the input); **caught N** count rises only while TC rolls.
- [ ] With a coarse offset already set, samples converge tight (low spread); **Detection latency**
      seeds from the capture block — trim it until the mean stops drifting.
- [ ] Stop → the previous beat source is restored (no lingering audio-beat mode).

### §TC.4 Show length — end handle + clamp 🖥️
1. Programming tab → select the **Show**; or the full Show Manager. Find the **END** handle
   (blue = auto at content end) or use the **Length…** toolbar button.
- [ ] **Drag** the handle (hover shows a ↔ resize cursor) → grid-snaps; **Length…** →
      *Set length…* / *Fit to content* / *End at current playhead* all work.
- [ ] Set length **shorter** than content → red **past-end warning** hatch over the clamped cues;
      run it → those cues **do not fire**. *Fit to content* restores full playback.
- [ ] Save + reload → the configured length round-trips (auto shows no `Duration=` in XML).

### §TC.5 Park-at-end (the original bug) 🎚️
1. Follow MTC and let the timecode run **past** the show's end.
- [ ] The playhead **parks at the end handle** (no scrolling off into empty timeline) and tints
      **cyan**; the footer chip reads **✓ complete +Xs past end**.
- [ ] Rewind Logic back within the show → the cursor un-parks and re-syncs (yellow again).

### §TC.6 MTC source dropdown removed 🖥️
- [ ] The Show Manager toolbar no longer has the MTC **source** combo; the footer chip's bind
      menu is the single place to pick the universe (offset presets / Length button still there).

## Notes / known gaps (not bugs to file)
- Per-channel yield keys on **fixtures**, not individual channels: a new cue that
  touches a fixture drops **all** of that fixture's held channels.
- Only **Show** stops hold; bare scene/chaser stops still black out (by design for now).
- A **running** show cue still steals a VC-grabbed channel back (separate symmetric-LTP
  concern, not addressed by this holder).
- The show timeline embed is intentionally compact: markers, cue-level editing, undo,
  and a transport still live in the full **Show Manager** tab.
