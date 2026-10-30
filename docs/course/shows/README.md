# Course show files

| File | What |
|---|---|
| `ShowChoir-Classroom.qlcc` | The teaching show: the rig, groups, targets and base palettes, no productions. See [the teaching show](../rig.md). |
| `lesson-1-1-start.qlcc` | Lesson 1.1's start (the same as the classroom show). |
| `lesson-1-1-finish.qlcc` | Lesson 1.1 done: *Spring Show / 01-Everybody Scream*, cues Pose, Verse, Chorus, made current. Lesson 0.8 opens it too. |
| `lesson-1-2-start.qlcc` | Lesson 1.2's start (the same as 1.1's finish). |
| `lesson-1-2-finish.qlcc` | 1.2 done: the verse Lavender at 40%, and 1.0.3 *Scream* with cue looks Red, Full. |
| `lesson-1-3-start.qlcc` · `lesson-1-3-finish.qlcc` | 1.3: AT-W-ALL Warm White, Full, Aim Center in the verse; 1.0.4 *Solo* (Block) with the AT-Spots White, Full, Aim Soloist DS (their shutter opens by itself). |
| `lesson-1-4-start.qlcc` · `lesson-1-4-finish.qlcc` | 1.4: fades and a hold (Pose fade in 3 s, Verse 2 s, Scream hold 2 s and fade out 3 s, Solo fade in 3 s). |
| `lesson-1-5-start.qlcc` | 1.5 runs the show: 1.4's finish. Nothing is built, so there is no finish file. |
| `lesson-2-1-start.qlcc` · `lesson-2-1-finish.qlcc` | 2.1 (from 1.4's finish): the verse's StepTops released; 1.0.2.5 *Flash* (Cue-only) with StepTops White, Full. |
| `lesson-2-2-start.qlcc` · `lesson-2-2-finish.qlcc` | 2.2: *Chorus Colour* (a copy of Magenta, violet after the note) in the palette folder *01-Everybody Scream*, on the chorus's Steps and TrussC-RGB. |
| `lesson-2-3-start.qlcc` · `lesson-2-3-finish.qlcc` | 2.3: 1.0.5 *Bridge* (Block): the group set *Truss Sides* Blue, Full with a child Trusses SR in Amber; Steps Blue, 25% with a child US in White, Full. |
| `lesson-2-4-start.qlcc` · `lesson-2-4-finish.qlcc` | 2.4: 1.0.6 *Dance Break* (Block): Steps in Chorus Colour with *Step Sparkle* in White; StepTops Blue, 25%; TrussW-RGB Blue with *Truss Wave* (effect palettes in *01-Everybody Scream*). |
| `lesson-2-5-start.qlcc` · `lesson-2-5-finish.qlcc` | 2.5: 1.0.7 *Fan* (Block): Truss-Spots White, Full, aimed at the Riser Line; 1.0.8 *Button*: Spread −5, Red, fade in 1 s. |
| `lesson-2-6-start.qlcc` · `lesson-2-6-finish.qlcc` | 2.6: the Hits section (*Red Stab*, *Glow*, *Faces*) and its Virtual Console page, a Toggle button each. |
| `lesson-2-7-start.qlcc` · `lesson-2-7-finish.qlcc` | 2.7: the chorus's sub-cues 1.0.2.1 *Chorus*, 1.0.2.2 *Stomp* (Lanterns White, Full; Sub GO), 1.0.2.3 *Stomp Out* (Out; auto-follow 0.5 s). |
| `classroom-eos.asc` | Lesson 3.10: a small Eos show in USITT ASCII, written by hand for the course (not a real production): ten dimmer channels — front, back, a cyc, specials, and house, work, relay and backstage circuits, one labelled so the import guesses it wrong — a cue list of four cues and one sub. |

Each lesson starts from the one before's finish, so a student who missed
a lesson starts the next one from its own start file.

All are safe for a school network: no universe has an output, no fixture
has a network address of its own. `ui/test/courseshow`
(`theTeachingShowIsSafeAndComplete`) checks that on every test run, and
that every fixture opens with its own definition on a computer that has
only the app's.

## Teaching mode

The shows are safe on their own (see above), so they ship with **Teaching
mode off**. It is a flag saved in the show (**File ▸ Show Settings…**), and
Lesson 3.6's scramble needs it. To hand a class a copy that has it, use
**File ▸ Show Settings… ▸ Make Student Handout…**: Teaching on, real network
output held, an optional PIN (so a student cannot untick it by accident),
and the practice answer key left out of the venue file. Keep your own master
without it. The footer shows a **TEACHING** chip and Operate shows a quiet
notice (Lesson 1.5 is unaffected: no dialog). See
[Teaching mode](../../features/teaching.md).

## How they are made

They are made by the app, not by hand, so they are exactly what the app
would save:

1. **Derive the rig** from the real show (read only):

   ```sh
   python3 -I docs/course/tools/derive-classroom-rig.py \
       path/to/DanceHysteria-0.6.qlcc /tmp/classroom-rig.qxw
   ```

   This keeps the stage and a subset of the lights, renames them, packs them
   into 13 universes with no outputs, removes every network address,
   function, palette, group and show, and adds the groups and stage targets.

2. **Build the shows** with the course test, which opens the rig in the app
   (offscreen), types the base palettes with the Show Tree command line,
   makes the Aim palettes, saves `ShowChoir-Classroom.qlcc` and
   `lesson-1-1-start.qlcc`, then does Lesson 1.1 **with the mouse, as the
   handout says**, and saves `lesson-1-1-finish.qlcc`. Then
   `buildLessonShows` does Lessons 1.2–1.4 the same way, each from the
   one before's finish, and `buildLevelTwoShows` Level 2's lessons
   (`COURSE_LESSON=2.N` for one of them, so the others' files stay as
   they are):

   ```sh
   cd build/ui/test/courseshow
   COURSE_RIG=/tmp/classroom-rig.qxw QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildTheShows
   COURSE_BUILD=1 QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildLessonShows
   COURSE_BUILD=1 COURSE_LESSON=2.1 QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildLevelTwoShows
   ```

3. **Take the pictures** (every lesson's, into `docs/course/images/lesson-L-N/`),
   at 1x (1280×800; the committed ones are), in the Blue theme — in a run of their own
   (`lessonPictures` only), so nothing an earlier test did (an open Look
   Editor, Recent items) shows in them:

   ```sh
   COURSE_SHOTS=$PWD/../../../../docs/course/images \
       QT_QPA_PLATFORM=offscreen DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test lessonPictures
   python3 tools/png-colour-chunks.py docs/course/images   # from the repo root
   ```

   Lesson 3.6's pictures (`images/lesson-3-6/`) are taken by its own test,
   `lessonFocusAndCalibration`, with the same `COURSE_SHOTS` set; Level
   2's by `levelTwoPictures` (`COURSE_LESSON=2.N` for one lesson's).

Nothing here touches your settings or your fixture folder: the test runs
with a home folder and settings of its own.
