# Controller jobs

Status: invariants written 2026-11-15, before the build (the operator's
rule: the safety rules first, then the code, then sweeps). Decided by him
2026-11-15 (memory `feedback_operator-control-rules`, "Controller jobs").
Code: `ControllerJobs` (engine/src/controllerjobs.*), the jobs section of
the Controller Test (ui/src/controllertestdialog.*), the checks in
`PadRun` and `ProgrammerController`. Tests: engine/test/controllerjobs,
engine/test/padrunrules, ui/test/padbenchshow, ui/test/connectionstree.

## Why

One game controller does everything: it runs cues, steers the follow
spot and works venue focus. With two or three pads on a show ("we can get
multiple controllers and will likely have them") every pad does all of
it, so a follow-spot op's pad can arm the show and a focus walker's pad
can steer the spot. A job is what one pad is for.

## The jobs

| Job | What it covers |
|---|---|
| **Run cues** | Pad Run's map: the arm chord, GO / Back / Sub GO / Sub Back, the hit, RS (say where we are), and the View + Menu hold that switches the Run role |
| **Follow spot** | the follow spot's sticks (bound from the profile or by hand), its fine stick, its D-pad speed and size |
| **Focus** | venue focus (a focus-call cue's Menu start, its calls, "still at <venue>?"), Focus mode (View + Menu tap in Tools), Refine and the walk (Menu in Run) |

Outside every job, on every pad: the emergency stop (the four together)
and Centre Sticks (View + LS, moves nothing). A Virtual Console widget
patched to the pad's channels is the show's own input mapping and hears
the pad as it hears any input: jobs are about the pad tools, not the
universe.

## Who a controller is

A job is saved for one physical pad, so the identity must survive the
pad being switched off, a rescan, and the pad landing on another input
line or universe. In order:

1. **`mac:aa:bb:cc:dd:ee:ff`**: a Bluetooth address in the device's
   name. The HID plugin names a device "<maker> <product> (<serial>)"
   (`assembleDevicesName()`, plugins/hid/hidjsdevice.cpp); macOS gives a
   Bluetooth pad's address as its serial ("DE-4C-09-77-77-93" or with
   colons). Six hex pairs split by `:` or `-`, folded to lower case and
   colons.
2. **`port:<plugin>:<location>`**: no address. The plugin's
   `inputDeviceLocation(input)` (new, `QLCIOPlugin`; empty by default):
   the HID plugin on macOS gives the transport and IOKit location ID from
   the hidapi path ("USB 14200000"; the path is
   `<transport>_<vid>_<pid>_<location>`), on Linux the joystick's
   `/sys/class/input/jsN/device/phys` ("usb-0000:00:14.0-2/input0"), on
   Windows the HID device path (it carries the port's instance ID). It
   is the USB port: the same pad in another port is another controller.
3. **`name:<plugin>:<name>`**: neither. Two such pads of one model share
   their jobs; the Controller Test says so ("can't be told apart").

decided: the Bluetooth address beats the port, because a pad on Bluetooth
has no fixed port. The same pad on a cable has another identity (its
address is not in a wired pad's name): its jobs are set once per way it
connects. (can change: a pad's serial over USB, when one says it.)

## Invariants

1. **Defaults.** A controller with nothing saved has all three jobs. One
   pad, nothing saved: every control, armed or not, in every tool, does
   exactly what it did before jobs existed (a sweep against the old
   map, not an example).
2. **Per computer.** Jobs are this computer's settings (`QSettings`,
   group `controllerJobs`, one key per identity), never the show's: a
   saved show carries none, and a show opened on another computer gets
   that computer's jobs. A controller set back to all three jobs leaves
   no key.
3. **Identity.** Two pads with different addresses, or no address and
   different ports, are different controllers, whatever their names.
4. **The emergency stop is everyone's.** The four together stop the show
   from ANY connected controller, whatever its jobs, armed or not, a tool
   holding the pad or not (Pad Run rule 5). A controller with no jobs
   does nothing else: no command, no arm, no light change, no follow
   spot move, no tool step.
5. **Run cues off.** Every Run-map control on that pad does nothing but
   say **This controller doesn't run cues** (the notice, so the footer's
   chip shows it, and aloud): the arm chord, GO, Back, Sub GO, Sub Back,
   the hit, RS, the role hold. Nothing on any other pad changes.
6. **One runner.** The first controller to arm holds Run until it
   disarms (its chord, the stop, the pad lost, its Run cues job taken
   away, the role switched, a tool taking the pad). While it holds it,
   another pad's arm chord, GO family and hit are refused with
   **<name> is running cues**, and nothing on the runner changes: still
   armed, its held safety still held, its chord timing untouched.
7. **Follow spot off.** That pad's sticks never move a follow spot: the
   follow spot's binding skips it (the next pad with the job, else none),
   a stick bound to it by hand stops moving the spot, and its fine
   stick, D-pad speed and size do nothing.
8. **Focus off.** That pad cannot start venue focus (a focus-call cue's
   Menu), Focus mode, Refine or the walk, and cannot become the pad that
   owns a tool (Q135). Its presses while a tool runs are ignored (but the
   stop). Among the pads with Focus, the first to press Start still owns
   the tool; another is told "Another controller is in use."
9. **One status source.** `PadRun::state()`, `stateColour()`,
   `roleText()` and `stateChanged()` stay the only source. With more than
   one pad connected, `roleText()` armed names the runner ("Pad: ARMED ·
   DualSense (…77:93)"); the footer chip and the Run tab read it, nothing
   derives its own. Each pad's light shows its own state: the runner the
   state's light, every other pad its own colour, every pad red after
   the stop.
10. **Jobs changed live.** Taking Run cues from the runner disarms it (no
   button: rule 3's "with no button at all"); taking Focus from the pad
   that owns a tool lets the ownership go; taking Follow spot from the
   bound pad rebinds. A change is saved at once.

## Where it is set

Connections ▸ right-click the controller ▸ **Test Controller and
Jobs…** (it was Test Controller…; no new item). The Controller Test shows,
under the picture of the controller picked, **Jobs on this computer**:
three switches (Run cues, Follow spot, Focus) and **Known as** (the
Bluetooth address or USB port). The picker lists each controller with
that identity, so two identical pads read "DUALSHOCK 4 · Bluetooth
…77:93" and "DUALSHOCK 4 · USB port 14200000"; pressing a control lights
the picture of the pad in your hand only. While the test is open the pad
runs nothing (as before); the jobs apply when it closes.

## Tests (sweeps, not examples)

- Two pads, every job mix (eight), every control pressed alone and in the
  chords on each pad, armed and not: what each produces is exactly its
  jobs' (engine/test/padrunrules `jobsSweep`).
- One runner: pad A arms, pad B's every control; then A disarms and B
  arms.
- The stop from a pad with no jobs, armed elsewhere and not.
- Identity for identical pads (same name, two addresses; same name, two
  ports; neither).
- One pad, nothing saved: the sweep equals the pre-jobs sweep.
- Settings: written, read back by a new store, absent from a saved show.
