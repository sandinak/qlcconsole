# Teaching mode

Teaching mode marks a show as a **classroom show**. A show you hand to
students carries it. It turns on the practice features, keeps the show from
sending real network output to the school's lights, and says so when someone
runs it for real. It is saved in the show file, so it follows the show to the
student's computer.

It is a flag on the show, not a view: a classroom show can be in any
workspace view, and Teaching never changes which tabs you see.

## Turn it on

**File ▸ Show Settings…**, then **Teaching mode**. While it is on:

- A blue **TEACHING** chip sits in the footer in every view, and the window
  title ends in "(Teaching)". Click the chip for a menu: what Teaching
  mode does, the lesson handout (when the show names one), and the Teaching
  settings.
- **Practice (scramble)…** appears under **Focus** in the Lighting Studio
  and in the focus calls list. Without Teaching mode it is not there, and
  the engine refuses a scramble. Reports of a practice already made still
  export, so a teacher can grade one on a copy that is not a Teaching show.
  See [Practice: scramble and focus back](focus-calls.md#practice-scramble-and-focus-back).
- **Real network output is held.** Art-Net, sACN, OSC and WLED outputs are
  kept as patched but not opened, like a patch off the rig network, and the
  footer says "Output held — Teaching mode holds real output". A student copy
  cannot reach the school network.
- **USB and other hardware outputs are held too** (an Enttec or DMX USB
  dongle, MIDI out), so a student copy sends nothing to a rig plugged into
  the computer. They are kept as patched, like the network ones. Loopback,
  Dummy and a network output aimed at this computer itself are not held.
- **The patch asks first.** Adding fixtures to the patch, removing them,
  releasing or changing their addresses (Patch dialog, Remap, Import, RGB
  panel) shows "This is a Teaching show, and its patch is part of the
  lesson" with **Change the Patch** / Cancel. Staging fixtures in the
  Inventory does not ask, it changes nothing on the wire. Fixture
  Properties in the Lighting Studio asks the same way when its universe or
  address boxes move a fixture (Cancel puts the boxes back), and so does
  pasting a truss with its fixtures patched at the next free addresses.
- The **focus report** of a Teaching show says "Teaching show", the lesson, and
  whether a PIN was set, above the practice score.

**Let real output through (instructor)** in the same dialog opens the held
outputs again (network and hardware). Use it on the teacher's own show when a lesson does run lights.

**Lesson** and **Handout link** are optional. A link adds "Lesson 3.6 handout"
to the chip's menu.

## The PIN

A show has one PIN (Show Settings, [Show settings](getting-started.md#show-settings)).
With a PIN set, **turning Teaching mode off** and **letting real output
through** ask for it. A wrong PIN leaves things as they were.

The PIN also locks Design in Operate, as it always did. Untick **Going back to
Design asks for the PIN** to use the PIN for Teaching only: that is how a
handout is made, so a student can still leave Operate.

The PIN is saved as a salted hash. It keeps students from switching Teaching
mode off by accident or on a whim. It is **not a lock**: the show is a plain
text file, and a student with a text editor can delete the Teaching line. Keep
your own master copy without Teaching mode. If the PIN is forgotten, the master
is the way back. There is no back door.

## Running a Teaching show

Operate is a lesson in itself (Level 1, lesson 5), so a Teaching show goes to
Operate without a dialog. You get:

- A line above the tabs: "This show is in Teaching mode. Practice features are
  on and real output is held…", with a **Show Settings…** button. Once for each
  time the show is opened.
- An entry in the footer's **Not ready** list: "Teaching mode is on". It clears
  when the show goes back to Design or Teaching is turned off.

Only when real output has been let through **and** a network output is patched
(the show looks like a real one) is there a question as well: **Run this show
for real?** with **Turn Off Teaching Mode…** (asks for the PIN), **Operate in
Teaching Mode** and **Cancel**. It is asked once per opening. A start nobody
answers for — the `-p` option, **Open this show in Operate**, kiosk mode, web
access — only gets the line and the Not ready entry.

## Make Student Handout

**File ▸ Show Settings… ▸ Make Student Handout…** saves a copy for students.
Your open show, its file name and its settings are left as they are.

The copy is always a `.qlcc`, so its fixture definitions travel with it, and:

- Teaching mode is on, real output is held (untick **Hold real network
  output** to let it through).
- It carries a PIN if you give one. The PIN is for Teaching only, so a student
  can still go from Operate to Design. It never opens in Operate.
- **Leave out practice venues and their answer key.** The practice's answer
  key (the right calibration of every light) is in plain text in the venue
  file beside the show, so the handout leaves practices out. Do not hand out a
  show with a practice in progress.
- **Blank the "checked by" names** in the venue file.
- Lesson and handout link, filled from the settings.

Put the copy in the same folder as the show if the show uses pictures kept
beside it. [Collect for Transfer](shows.md) is the way to move a show with its
files.

## Reset to Handout

A student copy keeps, inside the file, the show exactly as Make Student
Handout wrote it (the whole show, deflated; the file grows by about a tenth).
**File ▸ Show Settings… ▸ Teaching ▸ Reset to Handout…** puts the open show
back to that: patch, looks, cues, Virtual Console, settings. It asks for the
PIN when the show has one, then asks once more, because the students' work is
replaced. The file on disk is not touched until you save; the show is marked
changed. The handout state stays in the show, so it can be reset again.

Only a show made by Make Student Handout has it. Your own master has none and
the button is off. Practice venues and the venue file are not part of it (the
handout leaves them out): a reset does not bring back a practice, or take away
the student's venue file.

## Limits

- Teaching mode is a classroom aid, not security. Scores are only as honest as
  the student and the file.
- QLC+ 4 and older qlcconsole builds skip the Teaching setting. A show saved
  through QLC+ 4 loses it.
- The flag is off for every show made before it existed.
