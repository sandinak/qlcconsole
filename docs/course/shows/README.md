# Course show files

| File | What |
|---|---|
| `ShowChoir-Classroom.qlcc` | The teaching show: the rig, groups, targets and base palettes, no productions. See [the teaching show](../rig.md). |
| `lesson-1-1-start.qlcc` | Lesson 1.1's start (the same as the classroom show). |
| `lesson-1-1-finish.qlcc` | Lesson 1.1 done: *Spring Show / 01-Everybody Scream*, cues Pose, Verse, Chorus, made current. Lesson 0.2 opens it too. |
| `lesson-1-2-start.qlcc` | Lesson 1.2's start (the same as 1.1's finish). |
| `lesson-1-2-finish.qlcc` | 1.2 done: the verse Lavender at 40%, and 1.0.3 *Scream* with cue looks Red, Full. |
| `lesson-1-3-start.qlcc` · `lesson-1-3-finish.qlcc` | 1.3: AT-W-ALL Warm White, Full, Aim Center in the verse; 1.0.4 *Solo* (Block) with the AT-Spots White, Full, Aim Soloist DS (their shutter opens by itself). |
| `lesson-1-4-start.qlcc` · `lesson-1-4-finish.qlcc` | 1.4: fades and a hold (Pose fade in 3 s, Verse 2 s, Scream hold 2 s and fade out 3 s, Solo fade in 3 s). |
| `lesson-1-5-start.qlcc` | 1.5 runs the show: 1.4's finish. Nothing is built, so there is no finish file. |
| `classroom-eos.asc` | Lesson 3.10: a small Eos show in USITT ASCII, written by hand for the course (not a real production): ten dimmer channels — front, back, a cyc, specials, and house, work, relay and backstage circuits, one labelled so the import guesses it wrong — a cue list of four cues and one sub. |

Each lesson starts from the one before's finish, so a student who missed
a lesson starts the next one from its own start file.

All are safe for a school network: no universe has an output, no fixture
has a network address of its own. `ui/test/courseshow`
(`theTeachingShowIsSafeAndComplete`) checks that on every test run, and
that every fixture opens with its own definition on a computer that has
only the app's.

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
   one before's finish:

   ```sh
   cd build/ui/test/courseshow
   COURSE_RIG=/tmp/classroom-rig.qxw QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildTheShows
   COURSE_BUILD=1 QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildLessonShows
   ```

3. **Take the pictures** (every lesson's, into `docs/course/images/lesson-L-N/`),
   at 2x, in the Blue theme, as the help's are — in a run of their own
   (`lessonPictures` only), so nothing an earlier test did (an open Look
   Editor, Recent items) shows in them:

   ```sh
   COURSE_SHOTS=$PWD/../../../../docs/course/images QT_SCALE_FACTOR=2 \
       QT_QPA_PLATFORM=offscreen DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test lessonPictures
   python3 tools/png-colour-chunks.py docs/course/images   # from the repo root
   ```

Nothing here touches your settings or your fixture folder: the test runs
with a home folder and settings of its own.
