# Course show files

| File | What |
|---|---|
| `ShowChoir-Classroom.qlcc` | The teaching show: the rig, groups, targets and base palettes, no productions. See [the teaching show](../rig.md). |
| `lesson-01-start.qlcc` | Lesson 1.1's start (the same as the classroom show). |
| `lesson-01-finish.qlcc` | Lesson 1.1 done: *Spring Show / 01-Opener*, cues Pose, Verse, Chorus, made current. |

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
   `lesson-01-start.qlcc`, then does Lesson 1.1 **with the mouse, as the
   handout says**, and saves `lesson-01-finish.qlcc`:

   ```sh
   cd build/ui/test/courseshow
   COURSE_RIG=/tmp/classroom-rig.qxw QT_QPA_PLATFORM=offscreen \
       DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test buildTheShows
   ```

3. **Take the pictures** (`docs/course/images/lesson-01/`), at 2x, in the
   Blue theme, as the help's are:

   ```sh
   COURSE_SHOTS=$PWD/../../../../docs/course/images/lesson-01 QT_SCALE_FACTOR=2 \
       QT_QPA_PLATFORM=offscreen DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src \
       ./courseshow_test.app/Contents/MacOS/courseshow_test lessonPictures
   python3 tools/png-colour-chunks.py docs/course/images   # from the repo root
   ```

Nothing here touches your settings or your fixture folder: the test runs
with a home folder and settings of its own.
