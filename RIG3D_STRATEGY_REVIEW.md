# Rig view vs QLC+ 5's 3D visualiser — strategy review

**Question asked:** *"They DO have a visualizer in qlc5, so much of this work may
have been done and tied in — is it worth doing an in-depth analysis so we're not
reinventing the wheel?"* — plus *"add in the comparison of adopting the QML UI
wholesale, OR because we're in a popout window can we keep it segregated."*

**Short answer:** yes to reading it, no to adopting it, and yes — the popout
window genuinely is the escape hatch if we ever want their renderer. But the
most valuable thing this review found is not architectural: it is that **their
per-head colour code is more correct than ours**, and ours has a live defect we
can fix in an afternoon.

Everything below is measured against this tree, not recalled.

---

## 1. What QLC+ 5 actually has

| Piece | Size |
| --- | --- |
| `qmlui/qml/fixturesfunctions/3DView/*.qml` | 8,420 lines |
| `qmlui/mainview3d.{cpp,h}` | 2,740 lines |
| GLSL fragment shaders | 717 lines across 16 files |
| Fixture bodies | Collada `.dae` meshes (`par`, `moving_head`, `scanner`, `strobe`, `hazer`) |
| Stage presets | `StageBox`, `StageRock`, `StageSimple`, `StageTheatre` |

It is a real GPU renderer, and a good one:

- **Qt3D** (`Qt3D.Core`, `Qt3D.Render`, `Qt3D.Input`, `Qt3D.Extras`) hosted in
  QtQuick via `Scene3D`. Present and installed here — Qt 6.11.1 ships
  `Qt3DCore/Render/Extras/Input` frameworks.
- **Requires OpenGL 3.3+.** There is a `3DViewUnsupported.qml` that renders a
  polite "3D View disabled — a graphics card with support for OpenGL 3.3 or
  higher is required", but **it is unreachable in this version**: the branch
  that selects it tests `qlcplus.is3DSupported`, and `App::is3DSupported()`
  (`qmlui/app.cpp:282`) is hardcoded `return true;`. So an unsupported GPU
  gets a blank view, not the message. Worth knowing before reading a black
  panel as "our build is broken".
- **Volumetric beams by raymarching.** `SpotlightConeEntity.qml` feeds a shader
  `raymarchSteps`, `smokeAmount`, `coneTopRadius`, `coneBottomRadius`,
  `distCutoff`, plus a **gobo texture** and rotation, and full
  `lightViewProjectionMatrix` shadow-mapping parameters. There is bloom
  (`grab_bright`, `downsample`, `upsample`) and FXAA.
- **A real depth buffer.** There is no depth-sorting code in that QML *at all* —
  no `DepthTest`, no painter's-algorithm queue — because the GPU does it.

## 2. What it does not have — and this decides the question

Measured: `qmlui/` contains **zero** references to `Truss`, `StagePlatform`, or
`FixtureRigProps`.

Trusses, towers, platforms, riser/deck/inside mounts, per-face head-layout
grids, stackable steps, clear/open tops — none of it exists in QLC+ 5. It offers
four fixed stage presets. Our structural model is this fork's own work:
~11,800 lines in `structurestudioview.cpp`, `programmingmanager.cpp`,
`programmercontroller.cpp`, `monitorfixtureitem.cpp`, `truss.h` and
`stageplatform.cpp` alone.

**So adopting their 3D view does not give us our rig.** It gives us a renderer
we would then have to re-teach our entire structural model to, in Qt3D entities
and QML, from scratch. The thing we have that they do not is precisely the thing
the last several weeks of work built.

## 3. The three options

### A. Keep the widgets renderer; take their ideas (recommended now)

- **Cost:** days, in the file we already own.
- **Gains:** the colour-model fixes in §5 immediately; optionally a software
  z-buffer (§4) to end the depth-artefact family for good.
- **Loses:** no volumetric haze, no gobo projection, no shadows.
- **Risk:** low. Everything stays testable headless, which is how every bug in
  this area has actually been caught.

### B. Segregate — host their 3D view in the popout only

This is the interesting one, and the answer to the question as asked: **yes, the
popout makes it possible, and more cleanly than expected.**

- `MainView3D` derives from `PreviewContext`, which is a plain `QObject`
  (235 lines) — *not* a QML-only UI class.
- Its coupling to QML-UI infrastructure is thin: `ContextManager` **0**
  references, `UiManager` **0**, `ListModel` **1**, `Tardis` **4** (all
  `enqueueAction` undo hooks, trivially stubbed). `FixtureUtils` (15) is a
  556-line static helper with no QML dependency.
- It already reads **`MonitorProperties`** (15 references) — the *same* engine
  model our rig view reads. Fixture positions, gel colours and head layouts
  would come from one source of truth, not two.
- Because the Rig Overview is **already its own window**, we would not have to
  mix QML into the widget hierarchy at all. A top-level `QQuickView` sidesteps
  the whole `QQuickWidget`-hosting-`Scene3D` question, which is the part that
  historically bites.

- **Cost:** substantial. Building the Qt3D entity graph for trusses, towers and
  platforms is the bulk of it, plus a QML build path in a project that currently
  compiles `ui/` *or* `qmlui/`, never both (see §6).
- **Gains:** correct depth for free, volumetric beams, gobos, shadows.
- **It builds. Verified, not assumed.** `cmake -Dqmlui=ON` against
  `/opt/homebrew/opt/qt6` compiles the entire QML UI with **zero errors**,
  producing a `qlcconsole-qml` binary with Qt3D linked and a 4.9 MB
  `mainview3d.cpp.o`. The QML UI is *not* bit-rotted, which was the outcome I
  expected and did not get.

  (Worth recording how nearly this review got that backwards: a first probe
  without `CMAKE_PREFIX_PATH` auto-detected **Qt 5** and produced 68 errors
  across 13 files, which reads exactly like an abandoned subsystem. The
  giveaway was one line — `did not find header 'QAttribute' in framework
  'Qt3DCore' (loaded from '/opt/homebrew/opt/qt@5/lib')` — since `QAttribute`
  moved from `Qt3DRender` to `Qt3DCore` in Qt 6. The code was fine; the probe
  was wrong. Note also that `CLAUDE.md` still says this machine has no `qt@5`;
  it does, and `check-all.sh` builds against it.)

- **VERIFIED (2026-09-10): it renders.** Branson built and ran it: the 3D view
  comes up and draws. Qt3D works on this machine under Qt 6.11, so the last
  technical unknown in option B is closed.

  His verdict on it, which matters as much as the rendering: *"it doesn't work
  the same way ours does with the moving capabilities — I like our gesture
  management better."* Our drag-to-swing / shift-drag-to-pan / wheel-zoom is
  better than theirs. That is worth recording because it inverts the usual
  assumption: their renderer is ahead, their *interaction* is behind, and
  option B would mean giving up the interaction to get the renderer.

- ~~Still unverified — deliberately:~~ *(closed above)* whether Qt3D `Scene3D` actually *renders*
  on this machine's GPU stack. That needs the built binary to be launched and
  its 3D view opened. I have not done it: a second QLC+ instance can seize
  MIDI devices and ArtNet universes, and there is a live show file open. **Run
  it yourself when the rig is quiet** — the 3D view lives under **Fixtures &
  Functions -> "3D View"** in that app's sub-toolbar; it is not a separate
  binary, and there is no standalone visualiser to launch. To build it (this is the durable form —
  note the explicit Qt6 prefix, without which CMake may pick up `qt@5`):

  ```sh
  cmake -S . -B build-qmlui -Dqmlui=ON \
        -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6/lib/cmake
  cmake --build build-qmlui -j"$(sysctl -n hw.perflevel0.logicalcpu)"
  build-qmlui/qmlui/qlcconsole-qml          # plain binary, no .app bundle
  ```

  It takes about 90 seconds on this machine.
- **Also unverified:** Qt3D's long-term support. It is shipped in 6.11.1 here,
  but Qt Quick 3D is the newer module and **is also installed**. Check
  upstream's position before committing years of work to Qt3D.

### C. Adopt the QML UI wholesale (not recommended)

The build is strictly either/or — `CMakeLists.txt`:

```cmake
if(qmlui)
    add_subdirectory(qmlui)      # QLC+ 5
else()
    add_subdirectory(ui)         # QLC+ 4 widgets — everything we have built
    add_subdirectory(main)
    add_subdirectory(fixtureeditor)
endif()
```

Switching means **deleting this fork's UI**: 148,237 lines of widgets code, of
which the programmer mode, the Programming tab, Fixture Manager's group/layout
editors, the Lighting Studio and the rig view are ours. QLC+ 5's UI has no
programmer mode, no palette/look model, no structural rig. We would be starting
the fork again on a different toolkit to gain a renderer.

There is no partial version of this: the option flag admits no middle ground.
Option B *is* the middle ground.

## 4. The depth-buffer question — the real reason to look

Every one of these was one bug, and they are all the same bug:

- half of every step blanking off-axis (pixels vs their own housing);
- "can only see the nearest" for fixtures inside a step;
- in-step fixtures painting over the tape on the outside;
- truss webbing punching through a deck face.

All of them are **one depth value per primitive**, which cannot describe a
polygon whose depth varies across it. Each was fixed with a layering epsilon —
four times now. That is a smell: the fixes are correct but the model underneath
is not, and the next one will arrive the same way.

Two ways out, and Qt3D is not the only one:

1. **Adopt a GPU depth buffer** (option B), or
2. **Add a software z-buffer to the existing painter.** Render the ops to an
   offscreen `QImage` with a parallel depth array. This keeps the widgets
   renderer, keeps everything headless-testable, and kills the whole artefact
   family. Cost is real (per-pixel fill in software, and we have measured that
   rasterisation already dominates the frame at ~43 ms), so it needs its own
   measurement before committing.

Option 2 deserves a costed spike before anyone reaches for Qt3D on depth
grounds alone.

## 5. What to take from them right now, regardless of the above

This is the concrete payoff, and it is not architectural.

### 5.1 There is already a per-head colour API. We hand-rolled one.

`Fixture` already exposes, in the engine we share:

```cpp
QVector<quint32> rgbChannels(int head = 0) const;
QVector<QVector<quint32>> rgbChannelSets(int head = 0) const;
QVector<quint32> cmyChannels(int head = 0) const;
uchar            channelValueAt(int idx);
quint32          masterIntensityChannel() const;
```

`fixtureHeadLiveState()` walks `mode->heads().at(head).channels()` by hand
instead. Using `rgbChannels(head)` would be shorter, faster, and would not have
produced the 49 ms-per-frame master-dimmer walk that turned out to be the
flicker.

(`rgbChannelSets()` is also the engine-side explanation of the palette
colour-set stacking found in TODO follow-on 23: a head can carry several RGB
triples, and a look's colours land on set *n*.)

### 5.2 Their colour model covers twelve primaries. Ours covers five. **This is a live bug.**

`QLCChannel::PrimaryColour` defines:

```
Red  Green  Blue  Cyan  Magenta  Yellow  Amber  White  UV  Lime  Indigo
```

`FixtureUtils::headColor()` handles RGB **and CMY**, then blends White, Amber,
UV, Lime and Indigo on top with `blendColors()`.

Our `channelSetLiveState()` switches on `Red`, `Green`, `Blue`, `White`, `Amber`
and the colour wheel — and falls through `default: break;` for the rest. So:

- **a CMY fixture never shows its live colour** — subtractive movers are common,
  and ours would sit at its gel colour forever;
- UV, Lime and Indigo fixtures likewise contribute nothing.

We also *add* White/Amber into RGB; they *blend*. Blending is the better model —
adding is what makes a white-boosted colour clip toward white.

**Action:** ~~port `headColor()`'s structure into `fixturevisualtraits.cpp`.~~
**DONE 2026-09-10.** `channelSetLiveState()` now reads all twelve primaries,
treats CMY as subtractive (DMX 0 on every flag is white light, not black),
blends White/Amber/UV/Lime/Indigo rather than adding them, and reads a
colour-mixing fixture with no dimmer channel as ON rather than guessing a level
from its flags. Revert-checked: with the CMY/UV/Lime/Indigo cases removed, a
CMY fixture flagged full cyan renders `#5aa0eb` — which is the untouched gel
fallback, i.e. its live colour was never read at all.

### 5.3 Small confirmations

- They default an undeclared lens to **10°**; we chose 14°. Independent
  agreement that a `<Lens>` of 0 must become *something* for a beam to exist.
- They read `phy.layoutSize()` for head grids exactly as we do — our reading of
  that field is right.

## 6. Recommendation

1. **Now (days):** do §5.1 and §5.2. The CMY/UV/Lime/Indigo gap is a real defect
   found by this review, and fixing it needs none of the architecture above.
2. **Next (an hour, when the rig is quiet):** finish the option-B spike. The
   build half is done and clean; what remains is building it per §3B, opening
   its 3D view, and seeing whether Qt3D renders here — plus checking upstream's Qt3D-vs-Quick3D
   position. That turns the last unknown into a fact.
3. **Then decide on depth, on evidence:** cost a software z-buffer against the
   Qt3D popout. Pick one. Do not keep adding layering epsilons — that is four
   already, and the model underneath them is wrong.
4. **Do not adopt the QML UI wholesale.** It is an either/or build, and the side
   we would be leaving is the side with all of this fork's work in it.

---

## 7. Costed spike: a software z-buffer (2026-09-10)

Recommendation #3 said to cost a software z-buffer against the Qt3D popout
before reaching for either. Done — a standalone prototype, same projection and
same depth convention as `StructureStudioView`, rendering the exact failure
case: a wide step face whose depth varies across it, with pixel quads sitting
1 mm proud of it.

### The trick that makes it cheap

Depth across a **planar** polygon is an affine function of screen x,y, because
`project()` and `viewDepth()` are both linear in world space and there is no
perspective divide. So one plane fit per polygon is **exact**, not an
approximation, and the inner loop is an add and a compare:

```cpp
// z = A*x + B*y + C, fitted from three vertices
double z = A * (xa + 0.5) + B * sy + C;
for (int x = xa; x <= xb; ++x, z += A)
    if (float(z) > zline[x]) { zline[x] = float(z); line[x] = rgb; }
```

### Measured — and the first measurement was wrong

The standalone prototype, 1806 flat polygons at 1400x850:

| approach | ms/frame | strip coverage |
| --- | --- | --- |
| painter's algorithm, AA on | 0.513 | 192 columns (~48%) |
| painter's algorithm, AA off | 0.303 | — |
| software z-buffer, no AA | 0.248 | 398 columns (100%) |
| software z-buffer, 2x supersampled | 1.33 | 100% |

That is a scene of small polygons, so it barely exercises fill rate. On the
REAL rig the picture looked much worse — until it turned out the comparison was
against a Debug build:

| build | z-buffer | old painter |
| --- | --- | --- |
| Debug (`build/`, `-O0`) | 52 ms | 43 ms — *looks like a regression* |
| **Release (`build-qt6-rel/`)** | **10.3 ms** | **34.0 ms** |

**Always measure a rasteriser optimised.** Qt's painter is a prebuilt optimised
library; hand-written pixel loops in this tree are compiled `-O0` by default,
and comparing the two in a Debug build flatters Qt by roughly 5x. The
intermediate conclusion — "my scalar loop is 53x slower than QPainter, abandon
this" — was an artefact of the build type, and was very nearly acted on.

### Real numbers, real rig, Release, 1906 primitives

| renderer | ms/frame |
| --- | --- |
| old sorted-primitive painter | 34.0 |
| depth buffer, no antialiasing | 10.3 |
| **depth buffer, 2x supersampled (antialiased)** | **16.5** |

**Twice as fast with antialiasing, three times without, and correct.** The
repaint self-tunes to twice the frame cost, so this takes the live view from
about 15 fps to about 30.

Per-pixel, QPainter's SIMD fill still beats a scalar loop by ~10x on large
polygons (82 ms against 8 ms for 2000 big quads). It does not matter: the rig
is not fill-bound, it is primitive-bound, and the depth buffer removes the sort
and the overdraw that dominated.

### What the port would actually touch

- **Per-vertex depth.** `emitPoly()` and friends carry one depth today and would
  carry world vertices instead. **17 call sites** in `structurestudioview.cpp`
  (`emitPoly` 5, `emitLine` 7, `emitDot` 1, `emitDots` 2, `emitLabel` 2). Small.
- **Translucency does not go away.** A z-buffer is order-independent only for
  opaque geometry. Beams and clear tops (4 `setAlpha` sites) still need the
  standard two-pass scheme: opaque first with depth write, then translucent
  back-to-front with depth *test* but no depth *write*. So the sort survives —
  for a handful of primitives instead of all 1800.
- **Text stays 2D.** Labels get drawn onto the resolved image afterwards, which
  is what you want anyway; depth-testing a label is not meaningful.
- **Antialiasing has to be bought back** by supersampling — costed above at
  1.33 ms, which is affordable.
- **A bonus worth having:** with a depth buffer, hit-testing becomes reading one
  pixel. That would replace the 5 `hitTest` geometry paths with something
  pixel-accurate, and those have been their own source of bugs (see the
  fixture-id-0 arc).
- **Everything stays headless-testable.** It renders to a `QImage` either way,
  which is how every bug in this area has actually been caught — and is exactly
  what option B would give up.

### Verdict

Do it. It is cheaper than the Qt3D popout by a wide margin, it keeps the
gesture handling Branson prefers, it keeps the headless tests, and it ends the
artefact family rather than adding a fifth epsilon to it. The Qt3D route buys
volumetric haze, gobos and shadows — real things, but not the thing that keeps
breaking.

---

*Method note: every number here was measured in this tree — `wc -l`, `grep -c`,
and a full `cmake -Dqmlui=ON -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6` build
that completed with zero errors. Two things are marked unverified rather than
guessed: whether Qt3D `Scene3D` renders on this machine at runtime, and Qt3D's
long-term support status. One earlier conclusion in this document was wrong and
has been corrected in place rather than quietly dropped — see §3B.*
