# CLAUDE.md

Guidance for working in this repo. This is a **fork of QLC+ 4.14.2** (Q Light
Controller Plus) — the Qt5 **widgets** desktop UI, not the QML UI. The fork adds
a *programmer-mode* workflow and a dedicated **Programming** tab for building
scenes/"looks". Most fork work lives on the `programmer-mode` branch.

## Build & run

CMake + Qt5 (homebrew `qt@5`), out-of-tree in `build/` (Unix Makefiles, Debug).

```sh
# Reconfigure only when CMakeLists/sources are added or removed:
cmake -S . -B build

# Incremental build (parallel). Run after editing sources:
cmake --build build -j

# Run. ALWAYS pass -o to open a workspace; positional args are ignored and
# can overwrite the .qxw with an empty file (see memory: qlcplus-cli).
build/main/qlcplus -o surfacetesting.qxw
```

- App binary: `build/main/qlcplus` (arm64 Mach-O).
- Test workspace: `surfacetesting.qxw` — reuse it across iterations; don't
  rebuild fixtures/scenes/chasers from scratch (see memory: test-workspace).
- `qmlui` is OFF; ignore `qmlui/` for this work.

## Architecture (where things live)

QLC+ splits into an **engine** (no UI) and a Qt widgets **ui** layer.

- `engine/src/` — Doc, Fixture, FixtureGroup, Scene, Chaser, Collection, EFX,
  function model + DMX. Pure logic; serializes to/from `.qxw` XML.
- `ui/src/` — Qt widgets: Fixture Manager, Function Manager, the Programming
  tab, consoles, editors.
- `main/`, `launcher/`, `fixtureeditor/`, `plugins/`, `webaccess/` — entry
  points and peripheral apps; rarely touched for this work.

### Fork hotspots

- **`engine/src/programmercontroller.{h,cpp}`** — all programmer-mode logic
  (palettes, group scenes, Save workflow). `Doc` has *thin forwarders* only;
  put new programmer/palette/group logic HERE, not in Doc (memory:
  programmer-controller).
- **`ui/src/programmingmanager.{h,cpp}`** — the Programming top-level tab:
  searchable/foldered palette + function source trees → a SceneGroupLooks
  canvas. Drag palettes→looks, groups→dynamic targets, fixtures→fixed targets;
  inline look editor (color / dimmer gradient / XY pan-tilt / gobo-shutter);
  live DMX/2D preview (Design mode).
- **`ui/src/fixturemanager.{h,cpp}`** — Fixture Manager: fixture-group folders,
  empty-group creation, the head-layout grid editor (drag fixtures onto cells,
  add a group as a block, move a sub-group as a unit).
- **`engine/src/fixturegroup.{h,cpp}`** — `FixtureGroup` model: `path` (folders),
  head layout, sub-group tagging (+XML).

### Conventions

- Scene/collection names follow `SS-PP.II-Description`; when duplicating, learn
  the pattern from siblings — don't hard-code (memory: show-naming-convention).
- Match surrounding QLC+ style (it's stock Qt5/C++; existing files set the bar
  for naming, comment density, license header).
- Engine code touched by live preview must be thread-safe: `Scene::write`
  fader-map build is guarded with `m_valueListMutex` (race vs `resetRuntime`).

## TODO / status

`TODO.md` tracks programming-tab / programmer work — Done items at the bottom,
active work at the top. Update it as features land (commits often say
"TODO: … done").

## Memory

Persistent notes live under
`~/.claude/projects/-Volumes-Ext-git-qlcplus/memory/` (indexed by `MEMORY.md`).
Check there for workflow conventions before re-deriving them.
