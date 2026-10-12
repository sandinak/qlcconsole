# What's new

The latest changes, newest first. Each links to the guide with the detail.

## October 2026

### Help

- **In-app help.** **F1** opens the guide for whatever is in front — the
  tab, the Rig Overview, Fixture Properties, the Patch dialog, the look
  editor, the power dialogs. **Help** lists the guides: Getting Started,
  What's New, Keyboard and Mouse, Effects, Troubleshooting, and all of
  them; search finds a word in every guide. The stock QLC+ reference is
  still there, online, as **QLC+ Reference** (⇧F1).
- New guides: [Shows (the timeline)](shows.md) — what tracks, items,
  cues and the show end are; [Getting started](getting-started.md) (starting the app and
  the ways a show comes together), [Keyboard and mouse](shortcuts.md),
  [Effects](effects.md) and [Troubleshooting](troubleshooting.md).

### Lighting Studio

- **The Rig Overview stands on centre stage** — on the floor, in the
  middle of the stage grid — and turns about it, instead of centring
  whatever the rig's bounding box was (which floated the floor half a
  truss-height below the middle).
- **The rig view shows a running position effect on an Aim look.** A head
  under a look with an Aim target was always drawn pointing at the target —
  so Stage Shape, or an orbit round a target, never showed. Selected, a
  look still shows where its target is; running, the heads are drawn where
  they're actually sent.
- **A drag only moves; Shift-drop attaches.** Dropping a fixture over a
  truss, pipe or deck no longer hangs it there unless you hold **Shift** as
  you let go; the outline turns green while it would, and updates the
  moment Shift is pressed or released. Coming off still needs no Shift, and
  fixtures dragged in from the Inventory are placed where they land.
  ([Mounting](lighting-studio.md#mounting-a-fixture))
- **Height counts when attaching.** A fixture mounted on something (the
  foot of a tower, a deck) is only offered a truss or pipe within half a
  metre of its own height — no more lights at a tower's base hung on the
  bar on top.
- **Mount on → Deck.** Stands every selected fixture on the deck under it
  (or a named one): on its top, upright, nothing floating, nudged inside a
  deck it sat on the edge of.
- **Fixtures under structures** draw faded with a dashed outline in the
  Top view, still clickable.
- **Rulers mark the middle** of whatever you drag — a drape's centre line,
  not where you grabbed it — and band any selection, not only fixtures.
- **Orientation for anything that pans or tilts**, even bar-shaped (a
  two-arm beam bar standing on a step); **Test orientation** names its
  centred pose for it — straight up, down or out.
- **Lanterns on hangers hang straight.** A face left over from an earlier
  mount drew some on their side; it's cleared when the show loads.
- **Rig Overview** (⇧⌘R) **follows the Studio's layers** — hide a layer,
  it leaves the overview — and the same shortcut, or ⌘W, closes it.
- **Faster rig view**: a 200-fixture show draws in about 9 ms a frame (it
  was 51).

- **A truss between two stands** — the truss editor's **Other end on
  stand**: it runs from one stand's top to the other's and follows them.
- **Hangers on the 2D plot** — arm and hook, double-click to edit.
- **A patch can be undone** — the Patch dialog's Apply is one ⌘Z, on the
  Fixtures tab or in the Studio.
- A **house electric** (horizontal pipe) lists its fixtures by distance
  **along** it, up to its length, rather than a boom's height.
- The **Network Topology** dialog works like every rig dialog: the plot
  follows as you edit, Cancel puts the network back, OK is one undo.
- The Virtual Console's **button matrix** and **animation** keys moved to
  ⇧⌘G and ⇧⌘N, off the app's Lighting Studio and Rig Overview keys.

### Patching

- **Patching fits fixtures into what's free, in one order.** The Patch
  dialog's Place now defaults to **into the free space**: the fixtures are
  addressed one after another around whatever else is in the universe —
  no more conflicts from moving a patched fixture's block onto its
  neighbours — in one **addressing order** you choose: by name, movers
  first then by kind, or as they hang. The same group always lands the
  same way. Any
  with no room are flagged and left where they were, with **Carry on into
  the next universe** or **Fit by changing modes** to finish the patch.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))
- **Change a mode in the Patch dialog**, patched fixture or not; Apply
  changes it, and ⌘Z puts it back. A different head count is allowed
  unless a fixture group lays out heads the new mode doesn't have.

- **Patch list** — every fixture's patch, where its universe goes and what
  it hangs on, to save as **CSV** or a printable **PDF** for the crew.
  **Fixtures → Patch List…**, or the Studio's **More** menu.
  ([The patch list](fixture-manager.md#the-patch-list))
- **Patch Others Like This.** Right-click a structure: the other
  structures of its kind with the same fixtures are patched as it is — each
  fixture at the address of its counterpart, in its own structure's
  universe. The Patch dialog offers the same **Match** whenever its fixtures
  span several structures.
  ([Patching](fixture-manager.md#patching-the-patch-dialog))

### Programming

- **92 WLED effects run on any fixture** — Android, Aurora, Blink, Blink Rainbow, Bouncing Balls, Bpm, Breathe, Candle, Candle Multi, Chase, Chase Flash, Chase Rainbow, Chunchun, Colorful, Colorloop, Colortwinkles, Colorwaves, Dancing Shadows, Dissolve, Drip, Dynamic, Dynamic Smooth, Fade, Fairy, Fill Noise, Fire 2012, Fire Flicker, Fireworks 1D, Flow, Glitter, Gradient, Halloween Eyes, Heartbeat, Hyper Sparkle, Juggle, Lake, Lightning, Loading, Meteor, Multi Comet, Noise Pal, Oscillate, Pacifica, Palette, Phased, Plasma, Police, Popcorn, Pride 2015, Railway, Rain, Rainbow, Rainbow Runner, Random Colors, Ripple, Ripple Rainbow, Rolling Balls, Running, Running Dual, Saw, Scan, Scanner, Sine, Sinelon, Sinelon Dual, Sinelon Rainbow, Solid, Solid Glitter, Sparkle, Sparkle Dark, Spots, Spots Fade, Stream, Strobe, Strobe Rainbow, Sunrise, Sweep, Tetrix, Theater, Theater Rainbow, Traffic Light, Tri Fade, Tri Wipe, TV Simulator, Twinkle, Twinklecat, Twinklefox, Two Areas, Two Dots, Washing Machine, Wavesins, Wipe — so a look with one runs it on its WLED
  devices and its other fixtures alike.
- **WLED effects only where they can run.** A WLED effect that only runs on
  a WLED device is offered only when the look's targets include one;
  without, the WLED tab lists the ones that run on any fixture.
- **A Programming toolbar that fits.** Highlight, Flash, Tap and Save
  Positions stay buttons; Park, Move in black, Beat (it reads the tempo)
  and More are drop-downs — one row, no scrolling.
- **The effect browser is a window of its own.** Once an effect is picked,
  its settings have the look editor's full width; **Change effect…** opens
  the browser, where every pick (and every arrow-key step) plays at once,
  and **Cancel** or Esc puts back the effect you had.
  ([The effect browser](programming-tab.md#the-effect-browser))

### Playback

- **GO on the beat.** A chase or cue list can hold a GO to the next beat
  or bar — of its speed master, or the main beat — so a cue pressed a
  little early lands on the music. The Next button shows a GO waiting.
  ([GO on the beat](getting-started.md#go-on-the-beat))

### Effects

- **Speed masters in the footer** — a button for each master something is
  on (an unused one neither shows nor flashes, there or in its window),
  reading its tempo and lit on its beat, with a green level bar while its
  rate is moved; a click opens the Speed Masters window at it
  (**View → Show Speed Masters in Footer**). Each master's **Inputs…**
  maps its Tap, Rate and Pause to a MIDI controller.
  ([Speed masters](effects.md#speed-masters))
- **Effects layer as on a desk.** A colour effect replaces the look's
  colour on its heads instead of adding to it (Cyan under a rainbow washed
  every hue pale); a dimmer effect dims what's there, the colour effect's
  included. ([Effects](effects.md))
- **Rainbow with a colour in the look sweeps that colour** — a wave of
  Cyan across the heads, no rainbow; with two or more, it sweeps through
  them.
- **A rainbow on a wash with no dimmer is a rainbow.** On an RGBW head with
  no master dimmer (a Cobra Wash in 13-channel mode, say) an effect's own
  dimmer put red, green, blue and white all at full under its colour — a
  rainbow came out white.
- **Spread.** One Spread row for every chasing effect — amount in cycles,
  in look order or across columns, rows or the stage, **out from or round
  the look's target**, forward, reverse, centre out or ends in, with wings
  and blocks — and a preview of the heads coloured by phase. Every
  movement effect takes it too (orbit, figure 8, ballyhoo, wave, sweep,
  wander, fan, spotlight chase…), and **Stage Shape** uses it for which
  head goes where on the line or shape.
  ([Spread](effects.md#spread))
- **Speed masters.** 4 to 16 tempo masters (Programming toolbar →
  **Beat → Speed masters…**), one of which can follow the **show timeline**
  (and so timecode): each its own tempo, tap, rate and pause, or following the
  main clock or another master at ×¼ … ×4. Put an effect, a look, a chase
  step, a chase or a collection on one; on a master an effect's rate counts
  per beat; a chase counting in beats steps on its master's beats.
  ([Speed masters](effects.md#speed-masters))
- **Effects keep the beat together.** Every effect that follows the beat
  reads one beat clock — two started at different moments used to stay
  that far apart for good. **Tap** now averages your last four taps and
  puts the beat on the last one, pulling every beat effect onto it.
  ([On the beat](effects.md#on-the-beat))
- **Text** — words on a pixel grid, scrolling or still in the middle, in
  the look's colours; settings that are words get a text box.
  ([Text](programming-tab.md#text))
- **Step Test Pattern** — a diagnostic card for LED-strip steps: strips
  counted, steps numbered and named, a spinner that stops if output
  freezes. ([Step Test Pattern](programming-tab.md#step-test-pattern))
- The RGB Matrix **Text** algorithm has a **Centered** style.
- **Much faster on big pixel rigs**: 96 strips × 64 pixels ticks in about
  8.5 ms (was 68), leaving the screen free to keep up.
- The **effect list sits beside its settings** in the look editor, so both
  fit on an ordinary screen.

### Views and windows

- **Connections holds still.** The list refreshes every few seconds to
  catch nodes coming and going; it no longer drops the universe you picked
  or scrolls back to the top when it does, and it waits while a right-click
  menu or dialog is open, so **Patch…** acts on the row you chose.
- **Right-click a tab → Open in Its Own Window.** Works on the first
  click, even when the app isn't in front (a Mac spends that click
  bringing the window forward, so a Shift-click there did nothing).
- **Shift-click a tab** to put it in a window of its own — a double-click
  no longer does it by accident. **Window → Move … to Its Own Window** does
  the same for the view in front, and the Window menu can turn the
  Shift-click off. ([Window menu](other-improvements.md#window-menu))
- **Functions has no tab** — **Window → Functions** (⇧⌘4) opens it in its
  own window. ([Window menu](other-improvements.md#window-menu))
- The **Window** menu lists every view, wherever it is, and puts
  broken-out ones back.
- **Connections**' Devices header is one row; the list gets the height.

### Earlier this month

- **Power**: a fixture's **maximum draw** — amps, proportional, measured
  state by state, set on many at once and copied to the same fixtures —
  holds a hungry fixture under its circuit.
  ([Maximum draw](other-improvements.md#a-fixtures-maximum-draw))
- **Connections**: only a universe's name is edited in place; select a
  universe to see what it's sending, live.
- A look on a fixture **group** sets only that group's own heads.
- A function deleted while running lets go of its channels.
- **Scenery** (cyc, walls, drapes with an opening, a proscenium, a rounded
  front), **fountains** (with a silk-flame fixture and a labelled diagram),
  **lasers and star projectors**, a beam split across the surfaces it
  crosses, group rotation, and count badges on towers.
  ([Lighting Studio](lighting-studio.md))
