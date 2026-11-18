# Build ↔ Show Tree: workflow parallels, effects, and nomenclature

*Read-only evaluation, 2026-11-17, of `/Users/branson/git/qlcconsole-048` (train/050b @ 8ec39b489). Sources: code, help docs, course, design docs, existing help shots. I did not run the app; where a finding comes from reading code rather than from a doc or a shot, it says **(by reading)** and should be checked once on the real path before anyone calls it a bug.*

---

## 0. The short version

The **engine** parallel is exact and already designed (WORKFLOW_DESIGN §3.2): a Show Tree **layer bakes to a scene**, a **cue to a collection**, a **cue list to a chaser** (show-tree.md:37-44). The **editor** parallel is mostly there too: one Workbench with the same add bar, the same ↩, preview and undo, docked in the Show Tree for cues (W3).

The disconnect you feel is about **effects** and **visibility**, not about the scene/collection/chaser model:

1. **Since Q228 (0.4.4), Build is not on screen in the default (qlcconsole) view** (CHANGELOG.md:341-342, getting-started.md:219-224). The tab where effects get their "room" (W4: the bench, **Try on**, **Make a layer**) is an Everything-view tab. Nothing in the Show Tree's UI says it exists. `App::showEffects()` (app.cpp:5626), the one route that lands on "the Library's Effects" in the qlcconsole view, is called only by a test (appviews_test.cpp:556). No menu item calls it, because the Window item was removed (Q97, app.cpp:3615-3616).
2. **There are four effect-authoring surfaces, and each previews, names and keeps the effect differently** (§3.2 below). The Show Tree gets the weakest preview (the Look Editor, with no fixtures until the palette is on a cue) on its most-taught path, and a **modal dialog** for the good one. That dialog is the docked Workbench pane WORKFLOW_DESIGN §3.4 promised, left unfinished: "A cue layer's are declined for now" (effectdesigner.h:258-265, effectdesigner.cpp:417-420).
3. **The Workbench's `+ Effect ▾ → Effect Palette…` doesn't do what its tooltip says (by reading).** The tooltip says "make it a layer here, beside the effects already there". In a cue or a collection it actually **unloads what you were editing** and opens the bench (workbench.cpp:2794-2834, `clear()` at :2824). From a collection, **Make a Layer cannot go back into the collection**: `EffectRequest` has only `fromCue`/`fromScene` (workbench.h:200-214).
4. **What Build makes enters a cue as a black box.** A Build scene of groups and palettes, which WORKFLOW_DESIGN §3.2 calls "the same shape" as a layer, becomes a ▣ "played as it is" layer when dropped on a cue (showtreeview.cpp:7487-7510). Only a *raw* scene gets **Capture into Palettes…** (showtreeview.cpp:8826-8836). Tracking, cue looks and **Who wins** can't see inside it.
5. **The same words mean different things** across the two halves: *layer*, *look*, *Edit Effect*, *+ Scene*, *Effects*, and *Build* itself (§4).

Nothing below needs a new tab, model or playback path. Every recommendation attaches to the Library, the Workbench, the designer or the help.

---

## 1. The two workflows today, step by step

### 1a. Build → Scene → Collection → Chaser (Everything view)

| # | Step | Exact UI | Source |
|---|---|---|---|
| 0 | Get to Build | Default view hides it. **View ▸ Workspace ▸ Everything**, or **Window ▸ Build** (switches to Everything, with a line to come back) | getting-started.md:219-224, 263-266 |
| 1 | Make palettes | **Build ▸ Palettes**, right-click → New…; or the Library's **+ Palette** (the Show Tree's own Library, lent to Build) | build.md:73-74, 126-139 |
| 2 | Make a scene | **Build ▸ Scenes**, **+ Scene** (navigator) → loads in the **Workbench**; drag groups and palettes onto it ("Targets" + "Looks") | build.md:29-38, 79-81; shot build-tab-layout.png |
| 3 | Make an effect | **Build ▸ Effects ▸ + New** → designer on the **bench**, **Try on ▾** groups → **Save in This Show…** (name open to type) or **Make a Layer ▾** (in the scene you came from / Into the Show Tree's Cue… / Into a Scene… / Into a new scene…); **+ New ▾** → EFX / RGB Matrix in their stock editors | build.md:372-411; effects.md:189-217 |
| 4 | Collect | Load a collection; **+ Scene** (From the Show… / New Empty Scene), **+ Effect ▾** (Effect Palette… / EFX / RGB Matrix), ▲ ▼, Remove, Own Copy, ▣ Open | build.md:246-279; workbench.cpp:543-620 |
| 5 | Chase | Select scenes → **+ Scene ▾ → Chaser from selection**, or drag scenes onto a chaser's row; walk it: **◀ Step 3 of 7 ▶**, Fade in / Hold / Fade out / Cue / Note | build.md:281-349 |
| 6 | Time | Scene: Timing in the editor pane; chaser step: the step header | build.md:225, 286-297 |
| 7 | Run | VC button / VC Cue List / ⌘↩ if **Main cue list** is ticked in the chaser editor | coming-from-qlc4.md:172 |
| 8 | Hand over | Workbench header **Record it as a cue in the Show Tree ▸**; chaser **Send to Show Tree…** (= Upgrade This Show… with that chaser ticked) | build.md:215-219, 328-335 |

**Function Manager route** (stock QLC+): **Window ▸ Functions** (switches view from qlcconsole). The stock Scene / Collection / Chaser / EFX / RGB Matrix editors. In Everything a click selects and a double-click opens; in Classic QLC+ the editor opens beside the tree. Show Tree functions are greyed with **Show in Show Tree** (build.md:486-513).

### 1b. Show Tree → cue list → cue → layers / looks / effects (default view)

| # | Step | Exact UI | Source |
|---|---|---|---|
| 0 | Get there | **Show Tree** tab (⇧⌘4) | show-tree.md:3 |
| 1 | List | **+ Cue List** (or ⇧⌘N on a production) | show-tree.md:48 |
| 2 | Cue | **+ Cue**, or **Record** (what's on stage → a new Block cue) | show-tree.md:48, 1413 |
| 3 | Layer | Drag groups from the **Library** onto the cue, or **+ Layer** | show-tree.md:524-577 |
| 4 | Looks | Drag palettes onto the layer; the page's attribute rows (Colour, Dimmer, Position, Beam family, Shutter, Effect) with **✎** and **⋯** | show-tree.md:697-742 |
| 5 | Effect (taught path) | Library **+ Palette → Effect** → the Look Editor's effect browser, **Done**, type a name → drag it onto the layer (it is seen only now) | course level-2/lesson-04/student.md steps 5-11 |
| 5′ | Effect (design path) | Layer right-click → **Design Effect…**, or the Effect row's **⋯ → Design a New Effect…** → **modal** "Design effect — cue · layer" window: In Context / Solo, **Keep on This Layer** / **Save in This Show…** | showtreeview.cpp:6081, 8854; effectdesigner.cpp:1575-1738 |
| 5″ | Effect (docked) | **✎ Workbench** → **+ Effect ▾ → Effect Palette…** (goes to the bench, see §3.3) / **EFX** / **RGB Matrix** (new function as a ▣ layer) | workbench.cpp:566-588 |
| 6 | Time | Cue page: fade in / out, hold; the effect row's **⋯ → Fade In / Out** | show-tree.md:125-141, 950-971 |
| 7 | Run | **Run** tab GO / ⌘↩ / Cue Runner widget | show-tree.md:2817 |
| 8 | Reuse | **Make Reusable…** (layer / cue); palettes; **Hits** for busking | show-tree.md:2418, 2523 |

### 1c. Every place a user crosses between the two

| From → To | Crossing | Notes |
|---|---|---|
| Build → Show Tree | **Record it as a cue in the Show Tree ▸** (Workbench header, a loaded look) | good: a clear hand-off |
| Build → Show Tree | **Send to Show Tree…** on a chaser → Upgrade This Show | good |
| Build → Show Tree | Bench **Make a Layer ▾ → Into the Show Tree's Cue…** | needs a cue selected in a tab you can't see while on Build ("(none selected)", effectdesigner.cpp:1205) |
| Build → Show Tree | Drag a Build scene / collection from Library ▸ Functions onto a cue | arrives as an opaque ▣ layer, even a look-scene (§3.5) |
| Show Tree → Build | **▣ Open** / **Open “X”…** on an opaque layer | docks the Workbench in the tree; outside the dock: "“X” is open in Build's Workbench" (showtreeview.cpp:9884) |
| Show Tree → Build | **✎ Workbench** / double-click / `wb` | the same Workbench, docked (no tab switch) |
| Show Tree → Build | **Detach to Plain Chaser…** | the hint names "Build ▸ Scenes ▸ Productions, in the Everything view" (showtreeview.cpp:10119) |
| Show Tree → Build | **Window ▸ Build** | a view switch: the only explicit door |
| Both | The Library (one widget, lent), the command bar (cue words answer "Show Tree only" in Build), one undo history, the programmer strip | good: real parallels |

---

## 2. Side by side

✓ = the same, ≈ = parallel with a difference worth knowing, ✗ = mismatch.

| Concept / action | Build (scene → collection → chaser) | Show Tree (layer → cue → cue list) | Match |
|---|---|---|---|
| **Container levels** | Scene → Collection → Chaser (step) | Layer → Cue → Cue list → Production | ✓ engine-exact (bakes 1:1, show-tree.md:37-44) |
| **Make a look** | Scene in the Workbench: targets + an ordered stack of looks (rows read *White*, *Full*, *Line*; fixtures listed one by one: build-tab-layout.png) | Layer: group(s) + **one row per attribute** (*Colour White*, *Dimmer Full*: show-tree-workbench.png) | ≈ same data, **different row grammar** (D4 vs Build's stack) |
| **Add a part** | **+ Scene** (a member) | **+ Layer** (groups); in the docked Workbench the bar's first button is **+ Scene**, which in a cue makes an *opaque* layer | ✗ the primary verb differs, and the docked bar leads with the tree's least-wanted add |
| **Add an effect** | Build ▸ Effects bench (Try on, Make a Layer) / **+ Effect ▾** | **+ Palette → Effect** (Look Editor) / **Design Effect…** (modal) / **+ Effect ▾** | ✗ four surfaces, §3.2 |
| **Preview an effect before it has a home** | Yes: bench on **Try on** groups | No: the Look Editor has no context scene in the tree (showtreeview.cpp:8009-8031; LookEditor `m_contextScene` unset) | ✗ |
| **Preview in context** | Collection plays whole; Solo n/a | Cue **in context** (tracked-in too) + **Solo** | ≈ (Build has no Solo on a collection) |
| **Name it** | Bench **Save in This Show…** opens the name to type (effects.md:63-66) | Dialog **Save in This Show…** names it after the effect, no name box; Library **+ Palette** names it "New…", F2 | ✗ |
| **One-off vs shared** | Make a Layer without saving = one-off copy | **Keep on This Layer** = one-off ("Color Chase · 0.0.3 Announcer AT-W-ALL"), **Make reusable** later | ≈ two words for one idea |
| **Classic effect (EFX / RGB Matrix)** | Named "New EFX 57", **filed with the collection** (workbenchstack.cpp:580-587) | Named "New EFX 57", **no folder** (treecuestack.cpp:542-563) | ≈ / ✗ neither follows the show's naming |
| **Reuse** | A scene in several collections: badge **also in 2**, **Own Copy** | **Make Reusable…** (⟲ ×N), **Unlink Here**; palettes | ≈ two mechanisms for the same job, by design (§3.2) |
| **Sequence it** | Chaser steps; **Chaser from Selection** | Cues in number order; **+ Cue**, **Record** | ✓ |
| **Time it** | Step Fade in / Hold / Fade out (Common / Default greyed) | Cue fade in / out / hold; effect **⋯ → Fade In / Out** | ✓ (effect fades are Show-Tree-only; Build's command answers "Show Tree only", build.md:161-165) |
| **Carry over** | None: a step plays only itself (HTP mix if collections nest) | Tracking, Block, Cue-only, includes, Release | — intended difference (Show Tree's reason to exist) |
| **Run it** | VC, chaser as main cue list | Run tab, Cue Runner, GO | ✓ |
| **Edit it later** | Click it in Build ▸ Scenes/Effects → Workbench | Click the cue → page; ✎ Workbench for depth | ✓ |
| **Edit an effect palette later** | Right-click → **Edit Effect** → the **designer** (programmingmanager.cpp:1238) | Library right-click → **Edit Effect** → the **Look Editor** (showtreeview.cpp:11048) | ✗ same words, different editor |
| **Find it again** | Navigator search over Palettes / Effects / Scenes at once (build.md:67-72) | Library filter; Effects = **Palettes ▸ Effects** only; EFX / RGB Matrix under **Functions** | ✗ "Effects" lists different things |
| **Where else it's used** | Header **Used in**; badge **also in N** | Page **Also in:**; Library **Used By ▸** | ≈ four phrasings |
| **Kind on the header** | **SCENE** chip | **CUE** chip | ✓ (good: extend it, §5 R9) |

---

## 3. The disconnects you named, answered

### 3.1 From the Show Tree, how does a user learn that Build is where custom effects are made?

**They mostly can't, and in the default view the help sends them to a tab they can't see.**

| Where a user might look | What it says | Evidence |
|---|---|---|
| Tab bar (qlcconsole view) | no Build tab | CHANGELOG.md:341 (Q228) |
| Window menu | **Build** is listed after the separator (hidden tabs) and switches view. There is no "Effects" item | app.cpp:3604-3617 |
| Show Tree toolbar | + Cue List, + Cue, + Layer, Record, ✎ Workbench, More, MIB: no effect entry | showtreeview.cpp:1255-1290; show-tree-workbench.png |
| Layer page **Effect** row | **⋯ → Design a New Effect…**: the one in-place cue, and a good one | showtreeview.cpp:6081 |
| Library | **+ Palette ▸ Effect** → Look Editor; the "No effect palettes yet" hint exists only on the unreachable `showLibraryEffects()` | showtreeview.cpp:11255-11273 |
| Look Editor / effect browser | no "Design" or "Build" pointer (no such string in lookeditor.cpp / effectbrowser.cpp) | grep |
| Help: show-tree.md | "**Still in Build** (a tab of the Everything view): designing effects (the browser, presets, WLED), `.qxfx` import and export…": half stale (the browser, presets, WLED and Import/Export are in the Look Editor on either tab, effects.md:71-80) | show-tree.md:2303-2305 |
| Help: effects.md | "Build ▸ Effects (Everything view; the Library's **Effects** heading in the qlcconsole view)" | effects.md:40, 45 |
| Course | never mentions Build, the designer or the bench; effects are taught only as **+ Palette → Effect → Done** | lesson-2-4 student.md steps 5-11 |

So a student finds effects through the Library, and a pro finds the designer only by right-clicking a layer. The **bench** (an effect with no home yet, on Try on groups, named, then placed) can't be reached from the default view at all, unless Window ▸ Build flips the whole view.

### 3.2 Can effects be visualized while in Build, and named there? And in the Show Tree?

There are four surfaces. This table is the core of the disconnect.

| Surface | Reached from | Plays on the rig | Name at birth | Keeps as |
|---|---|---|---|---|
| **A. Look Editor + effect browser** | Library **+ Palette → Effect**; Library **Edit Effect** (tree); clicking an Effect row in the docked Workbench (workbench.cpp:2743 falls through, by reading) | only where a cue already uses it and that cue is previewed; a new one plays on nothing | "New …" then type in **Name** (course step 7) | a named palette |
| **B. Design effect dialog (modal)** | layer right-click **Design Effect…**; Effect row **⋯ → Design (a New) Effect…** | **yes**: the layer's fixtures, **In Context** / **Solo** | **Save in This Show…** names it after the effect, no box (effects.md:63-67) | **Keep on This Layer** (one-off, "Fire · 1 Chorus Towers"), **Save in This Show…**, **Update “x”** |
| **C. The bench (Workbench pane)** | Build ▸ Effects **+ New** / click an effect palette; Workbench **+ Effect ▾ → Effect Palette…** | **yes**: the **Try on** groups (remembered by name, defaulting to the show's first group, effectdesigner.cpp:398-413) | **Save in This Show…** with the name open to type | **Save in This Show…**, **Update**, **Make a Layer ▾** (one-off unless saved) |
| **D. Stock EFX / RGB Matrix editors** | **+ Effect ▾ → EFX / RGB Matrix**; Build ▸ Effects **+ New ▾** | yes (as a function) | "New EFX 57" | a function |

Answer: **yes in Build** (C is the best "see it, then name it" tool in the app). **In the Show Tree, only B shows it**, and only after you've committed to a layer, in a modal window that hides the tree and the Library. On the path the course teaches (A), a student names an effect before seeing it on any light ("The steps twinkle" happens only at step 8, after the drop).

### 3.3 `+ Effect ▾ → Effect Palette…` in the Workbench (by reading; verify on the real path)

- **Tooltip:** "Design an effect palette and make it a layer here, beside the effects already there" (workbench.cpp:576-578).
- **What it does:**
  - It calls `designEffect(EffectRequest())`. The request has no `cueId`, so it takes the bench branch, which calls **`clear()`**: the loaded collection or cue is unloaded (workbench.cpp:2794-2828).
  - The designer opens on the **Try on** groups (last used), not on the selected layer's.
  - From a **cue**, `fromCue` is set, so **Make a Layer in cue N** makes a *new* layer on the Try on groups, not "beside the effects already there" on the selected layer.
  - From a **collection**, neither `fromCue` nor `fromScene` is set (`fromScene` needs a loaded *Scene*, :2815-2819), so Make a Layer offers *Into the Show Tree's Cue… / Into a Scene… / Into a new scene…* and **not the collection you were in**.
  - Unknown: what `clear()` does to a Workbench that is **docked** in the Show Tree. It may leave the dock empty. Worth one check.
- **The test doesn't catch it:** `workbench_test::newEffectsAreMadeOnTheSelectedLayer` checks only that the tooltip contains "beside" (workbench_test.cpp:524-527), not where the effect lands.
- **Why it happens:** the factory declines cue requests ("declined for now (the Show Tree's Design effect… dialog does those)", effectdesigner.h:258-265). So the layer-path `Workbench::designEffect()` (:2766-2792) always returns false. That is also why clicking an Effect row in the docked Workbench opens the Look Editor, not the designer.

This is the most concrete instance of "it doesn't make it clear when generating custom effects". The design recorded in WORKFLOW_DESIGN §3.4 ("Designing an effect for one layer … the editor pane becomes the effect designer … ↩ to the Show Tree") was half-built: Build got it, the docked cue did not.

### 3.4 When a custom effect is made, where does it land, what is it called, and how is it found again from the Show Tree?

| Made by | Lands | Called | Found again from the Show Tree |
|---|---|---|---|
| **Save in This Show…** (B or C) | the show's effect folder (`suggestPaletteFolder`), not the song folder the course asks for ("Why make effect palettes in the song's folder", lesson 2.4 exit ticket 4) | the effect's name ("Sparkle"); typed over only in Build | Library ▸ All ▸ Palettes ▸ Effects (Group by Type) or that folder; filter by name |
| **Keep on This Layer** (B) / unsaved **Make a Layer** (C) | inline on that layer (D11 / D21) | "Effect · cue layer", follows renumbering | **only via its cue**: Library **Recent** holds it for this session only (show-tree.md:2226); never in All, by design |
| **+ Palette → Effect** (A) | the folder selected in the Library | "New …", renamed in the Look Editor | as any palette |
| **+ Effect ▾ → EFX / RGB Matrix** in a cue (D) | no folder (treecuestack.cpp:542-563) | **"New EFX 57"** | Library ▸ All ▸ **Functions (played as they are)**; *not* under Palettes ▸ Effects |
| the same in a Build collection (D) | filed with the collection | "New EFX 57" | Library ▸ Functions; Build ▸ Effects ▸ QLC+ effects |

Gap: the Library's **Effects** means "effect palettes", while Build ▸ Effects means effect palettes *and* EFX / RGB Matrix (build.md:75-79). A classic effect a student made in a cue is filed under a different heading ("Functions") with a generic name.

### 3.5 Dead ends, hidden steps, same-thing-different-name

| # | Finding | Evidence |
|---|---|---|
| D-1 | **A Build look-scene enters a cue as ▣.** `dropFunctions` makes every function an opaque layer. **Capture into Palettes…** is offered only when the scene has raw values. So a blue scene from Build can't be opened up into a layer, although Upgrade This Show does exactly that for chaser steps ("A scene of groups and palettes became a layer", show-tree.md:255) | showtreeview.cpp:7487-7510, 8826-8836 |
| D-2 | **Two designers for one job**: the modal dialog (tree) and the pane (Build). Their buttons differ: Keep on This Layer vs Make a Layer ▾; Cancel shown vs not | effectdesigner.cpp:254-276, 1575-1738 vs :417-430 |
| D-3 | **"Edit Effect"** opens the designer in Build and the Look Editor in the Show Tree | contextmenus.cpp:286-290; programmingmanager.cpp:1238; showtreeview.cpp:11048 |
| D-4 | **Stale help and tooltips** that describe the old + Effect menu: build.md:252-255 and show-tree.md:209-210 ("+ Effect ▾ → EFX or RGB Matrix"); workbench.cpp:1266 tooltip ("A new EFX or RGB Matrix, added as a layer"); show-tree.md:2303-2305 ("Still in Build"); help shot build-effects-menu.png shows **Design / edit…** and **Save as palette…**, which are now **Edit Effect** and **Save in This Show…** | files cited |
| D-5 | **`App::showEffects()` is unreachable** except from a test, so the qlcconsole view's "Library ▸ Effects" landing and its "No effect palettes yet" hint never show | app.cpp:5626; appviews_test.cpp:556,573 |
| D-6 | **Both Build help shots and getting-started's main-window shot show the Build tab in the tab bar**, i.e. the Everything view. The default view a student opens doesn't look like that | build-tab-layout.png, show-tree-workbench.png |
| D-7 | **The docked Workbench's add bar has no + Layer.** Its first button **+ Scene** makes an opaque layer in a cue; group layers come only by drag or Selection ▸ New Layer from Selection | workbench.cpp:552-562; show-tree.md:204-211 |
| D-8 | **Getting-started Workflow 1 step 10** is the only place that tells a new user Build exists ("scenes and collections are made in the Everything view's Build tab"), with no word on effects | getting-started.md:409-412 |
| D-9 | **New classic effects ignore the naming convention** (SS-PP.II-Description; one-offs already follow the cue) | treecuestack.cpp:563; workbenchstack.cpp:587 |

---

## 4. Nomenclature

### 4.1 Term by term

| Term | In qlcconsole | Elsewhere | Collision / confusion | Verdict |
|---|---|---|---|---|
| **Show Tree** | the tab: Production → Cue list → Cue → Layer, plus Hits and Reusables | no console uses it; "tree" is a software word | describes the widget, not the job; a newcomer looking for "cues" doesn't connect it; the course has to teach it | keep (see 4.2) |
| **Production** | the tree's root; the current one plays on GO | theatre: the whole show; MA: none; Eos: the show file is "show" | the default production is named **Show** (show-tree.md:2374), next to the *Shows*/Timeline tab and the QLC+ *Show* function | fine; consider a default name other than "Show" |
| **Section** | a song inside a list, from the first cue number | music: verse/chorus, which the course also uses (0.5 song map) | "song section" (part of a song) vs "section" (a whole song) | ok, the glossary defines it; watch for it |
| **Cue list** | a list in the tree, Tracking or Non-tracking | Eos *cue list*, QLab *cue list*, MA *sequence*, Hog *cuelist*, ChamSys *cue stack* | **QLC+'s Virtual Console *Cue List* widget** plays a *chaser*; the Show Tree's lists play through it only via the Cue Runner (D26) | correct industry word |
| **Cue** | a numbered state change, tracking | Eos/MA/Hog/QLab: the same. **ChamSys: a "cue" is a stored look**, roughly a QLC+ *scene*; its sequence is a *cue stack* | Simple Desk's raw **cue stack** cues (WORKFLOW §1.1) | correct |
| **Layer** | **Show Tree**: group(s) + one value per attribute. **Build** (build.md:1229): "a scene, or a function that plays as it is"; a collection's members | MA3 "layers" are programmer value / fade / delay layers; video and graphics software: z-order | **one word, two grains**: in Build a whole scene is "one layer", in the tree a scene *is* a baked layer. Decided (WORKFLOW Q13: rows as layers), so it holds, but D-1 makes the gap visible | keep; close D-1 |
| **Look** | course 0.1: everything at one moment; the tree: one attribute's value ("cue look"); Build: a palette applied in a scene (build.md:1226); the **Look Editor** edits palettes; busk "Looks" became **Hits** | lighting talk: "the look" = the whole stage picture | **four meanings**; the student's first meaning (the whole stage) is the one the app uses least | the biggest word-level hazard; no rename, but help should say "a look row" / "cue look" every time |
| **Scene** | a QLC+ function; a Build object; also the tree's hidden baked layer | **theatre: Act 1 Scene 2** (the calling script's SCENE lines, show-tree.md "The calling script"); Hog: a single-cue cuelist; recent Eos versions can label a cue as starting a "scene" in a list | high for a theatre school: "scene" first means a part of the play | keep (QLC+ compatibility); disambiguate in the course |
| **Collection** | a QLC+ function that plays members together; a cue bakes to one | QLC+ only | none for students (never taught) | fine, QLC+-only |
| **Chaser** | a QLC+ function of steps; a cue list bakes to one | QLC+ / older consoles: a looping chase; "chase" = an effect | in QLC+ a chaser is both a cue list and an effect (WORKFLOW §1.3.5) | fine; the Upgrade converter already tells them apart |
| **Effect** | an effect palette (script); "QLC+ effects" = EFX / RGB Matrix; an "Effect" attribute row per kind | Eos *effects* (step / absolute / relative); MA2 *effects*, MA3 *phasers*; ChamSys *FX* | three engines under one word; the Library's "Effects" ≠ Build's "Effects" (D-4, §3.4) | keep the word; list both kinds under it everywhere |
| **Palette** | any type, one folder model | Eos *palettes* (IP/FP/CP/BP) + *presets*; MA *presets*; Hog / ChamSys *palettes* | none | right word |
| **Build** | the tab (was Programming, Q23-ish / P-D1) | course glossary: **"Build: light that grows over several bars"** (GLOSSARY.md:82); dev talk "a build" | latent: the course never names the tab; if it ever does, students meet two meanings | keep; one glossary line |
| **Workbench** | one editor for "a stack of things that play together" | none (MA "Workspace", Eos "Tab") | none; clear | keep |
| **Bench / Try on** | the designer with no home, on the Try on groups | none | "Workbench" vs "bench" read as the same thing | fine; the help could say "the effect bench" |
| **Record / Snapshot / ⌥⌘D** | Record = a new cue; Snapshot = live DMX into the scene; ⌥⌘D = save stage as scene | Eos *Record* / *Update*; **Eos *Snapshot* recalls a screen layout** | Snapshot is an Eos false friend | low |

### 4.2 Is "Show Tree" the right name?

| Option | For | Against |
|---|---|---|
| **Show Tree** (today) | unique, so no false friend; it holds more than a cue list (productions, several lists, hits, reusables); already in the course, help, CHANGELOG and 0.5.0 | names the widget's shape, not the job; an Eos/MA/QLab user looks for "cues"; must be taught |
| **Cues** | what every console and QLab user says; students say "the cues"; short | also the name of one level (cue); hits and productions aren't cues |
| **Cue List(s)** | industry word (Eos, QLab) | collides with the level *cue list* and with QLC+'s VC **Cue List** widget; a production has many lists |
| **Show** | plain | collides with the **Shows** (Timeline) tab, QLC+'s **Show** function and the default production "Show" |
| **Cue Stack** | ChamSys / Simple Desk word | collides with Simple Desk's raw cue stacks and the engine `CueStack` class |
| **Script** | theatre-native | collides with the calling script and QLC+'s **Script** function |

**Opinion:** keep **Show Tree**. Every alternative collides with a QLC+ object this fork keeps first-class (Strategy: old habits stay first-class). The cost of the name is a findability cost, and it is cheap to pay down without renaming: make "cues" visible where a newcomer looks. Examples: the tab's tooltip, *"Show Tree: your cue lists, cues and hits"*; the Window menu and the F1 topic title saying the same. The pair that matters is **Build = make things, Show Tree = put them in order and run them** (WORKFLOW §0.1, §3.1). Since Q228 hid Build, that pair is only half visible, which is the deeper issue (§3.1).

---

## 5. Recommendations, ranked by ROI

Effort: **S** ≈ a day or less, **M** ≈ 2-4 days. Each attaches to an existing model: no new tab, model or playback path, and the menu count stays flat or falls. "Help / course" says the text it needs.

| Rank | Change | Attaches to | Effort | Who gains | Help / course text |
|---|---|---|---|---|---|
| **R1** | **Finish W4 for cues: design a cue layer's effect in the docked Workbench pane.** Accept cue requests in the factory (or host `EffectLayerDialog`'s designer in the pane); clicking an Effect row and **+ Effect ▾ → Effect Palette…** on a cue layer open it on *that layer's* fixtures, In Context / Solo, Keep on This Layer / Save in This Show, ↩. Retire the modal dialog: right-click **Design Effect…** opens the same pane (docking the Workbench). Fix the tooltip, and add a test that the effect lands on the selected layer | Workbench + designer (WORKFLOW §3.4, as decided) | M | both: one designer, seen in context, without a modal window | show-tree.md "Effects" + "Working on a cue in the Workbench": one paragraph; effects.md "Open it from any of three places" → two |
| **R2** | **`+ Effect ▾ → Effect Palette…` in a Build collection lands in that collection.** Add `fromCollection` to `EffectRequest`; Make a Layer's first choice = "in 0.0.4-SHOUT" (a new scene layer on the Try on groups, or on the selected layer's groups). Don't `clear()` the collection behind the bench | Workbench / bench | S | QLC+ habit-2 users | build.md "Making effects", one line |
| **R3** | **Give the default view its effects door, with no new menu item:** Library **+ Palette ▸ Effect** opens the **designer** (the bench, docked in the Show Tree's Workbench; Try on = the selected layer's groups, else the selected cue's layers, else last used) instead of the bare Look Editor. Library right-click **Edit Effect** opens the designer too, as in Build (fixes D-3). Wire the existing `showLibraryEffects()` hint | Library + docked Workbench (Q228's "what opened in Build opens in the Show Tree") | M | students: see it, then name it | course lesson 2.4 steps 5-7 rewritten ("pick it, watch it on the steps, name it, Save in This Show"); show-tree.md "Palettes in the Library" |
| **R4** | **Sweep the stale words** (D-4): build.md:252-255, show-tree.md:209-210 and :2303-2305, workbench.cpp:1266 tooltip; re-shoot build-effects-menu.png; add a qlcconsole-view shot of the docked Workbench | help, tooltips | S | everyone reading help | as listed; `tools/help-shots.sh` |
| **R5** | **Name and file new EFX / RGB Matrix like one-offs**: "EFX · 1.0.3 Chorus Towers" in the production's folder (cue) or with the collection (Build); follow renumbering as one-offs do | TreeCueStack / FunctionStack `addNew` | S | both; findable again (show-naming convention) | show-tree.md "Adding layers", one line |
| **R6** | **One "Effects" in the Library**: under Palettes ▸ Effects also list "QLC+ effects (EFX, RGB Matrix)", as Build ▸ Effects does; Functions keeps them too | Library | S | both | show-tree.md "The Library" |
| **R7** | **A Build look-scene dropped on a cue becomes a layer** (its groups + looks copied as the cue's own, palettes shared), with ⌥ to play it as is (▣). On an existing ▣ look-scene, the same menu slot as **Capture into Palettes…** reads **Make It Layers…**. Reuse Upgrade's step-scene reader and the `same()` check | Show Tree drop + Capture slot (WORKFLOW §3.2 "same shape") | M | pros who build in Build, then cue | show-tree.md "Adding layers"; build.md "On a Show Tree cue" |
| **R8** | **Same row grammar in the Workbench**: Build scene look rows show their attribute too (*Colour · White*, *Dimmer · Full*), in the stack order Build needs; a scene's targets fold to the group when the whole group is targeted | ProgrammingOutline rows | S-M | students moving between views; the screenshots' biggest visual mismatch | none (it explains itself) |
| **R9** | **Say the mapping on screen once**: the Workbench header's kind chip for a cue gets a tooltip (*"A cue plays as a collection; each layer as a scene; the list as the chaser ‘Main’"*); a SCENE chip's tooltip says *"In the Show Tree this is a layer"*. Add a side-by-side table to show-tree.md (§2 of this report, trimmed) | header chip (exists) | S | QLC+ users; teachers | show-tree.md "You never build the playback by hand" gains the table; coming-from-qlc4.md links it |
| **R10** | **Docked Workbench add bar in a cue**: lead with **+ Layer** (from the Library selection, as the tree's), keep + Scene in its menu. The bar then reads the tree's words in a cue and Build's in a collection | Workbench add bar (`updateAddBar` already branches) | S | Show Tree users | show-tree.md:204-211 |
| **R11** | **Course**: lesson 2.4 gains "Design it on the layer" (right-click the Steps layer → Design Effect…, In Context vs Solo, Keep on This Layer vs Save in This Show). The glossary gets *Effect designer*, *One-off effect*, and a line under **Build** ("also the Build tab, in the Everything view: where scenes, collections and chasers are made, the QLC+ way") and under **Scene** ("in the app, a QLC+ scene, not a scene of the play") | course | S | students, teachers | lesson 2.4 student/handout/teacher guide; GLOSSARY.md; `check-course-ui-names.py` |
| **R12** | **One phrase for "where else"**: *Used by* (palettes), *Used in* (functions), *Also in* (▣ page), *also in N* (badge) → settle on **Used in** for places and **Used by** for VC / triggers, everywhere | strings | S | everyone | none |

Ordering rationale: R1-R4 remove the "it doesn't make it clear" feeling directly (the designer is where you are, and the words match). R5-R6 make the result findable. R7-R10 make the two halves look like one model. R11-R12 are the teaching and polish layer.

### Owner decisions (each with a recommended answer)

1. **Q-E1. One effect designer, in the Workbench pane, for cue layers too (R1)?** Recommended: **yes**. This *completes* WORKFLOW_DESIGN §3.4 ("the editor pane becomes the effect designer… ↩ to the Show Tree") rather than reversing it. The modal dialog was the stop-gap the code comment calls "for now".
2. **Q-E2. In the qlcconsole view, should Library + Palette ▸ Effect (and Edit Effect) open the designer instead of the Look Editor (R3)?** Recommended: **yes, the designer, docked; the Look Editor stays for every other type**. ⚠ This **changes recorded 0.4.4 behaviour** (CHANGELOG.md:342, under Q228: "opening or editing a palette (effect palettes too)… open it in the Look Editor"). Worth revisiting because Q228 hid the only surface that previews an effect before it has a home. The course would need its steps 5-7 rewritten.
3. **Q-E3. A Build look-scene dropped on a cue: a layer by default, or ▣ as today (R7)?** Recommended: **a layer by default, ⌥ for ▣**, with the original scene kept. Consistent with §3.2 ("the same shape") and Upgrade; it doesn't touch D13 (nothing generated is hand-edited). Trade-off: a layer is a copy, so later edits in Build no longer reach that cue. That is the same trade as Upgrade's, and **Used in** should say so.
4. **Q-E4. Show Tree name.** Recommended: **keep it** (4.2); add "cue lists, cues and hits" to its tooltip, Window entry and F1 title. No rename.
5. **Q-E5. Should Build come back into the qlcconsole view's tab list?** Recommended: **no** (Q228 stands, and the menu count stays flat). Bring its *effects* half to the Show Tree instead (R1 + R3); scenes, collections and chasers stay an Everything / QLC+-habit workflow, which is what they now are. Revisit only if R1-R3 don't close the gap in class.

---

### Not verified / caveats

- §3.3 and D-7 come from reading code paths, not from running them. Check them once on the real path (a docked cue and a Build collection, then **+ Effect ▾ → Effect Palette…**) before filing them as bugs. That includes what `clear()` does to the docked Workbench.
- Industry usage (§4.1) is general practice. The Eos "scene" label and MA3 "layers" are stated loosely; neither changes a recommendation.
