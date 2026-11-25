# Surface simulator: testing control surfaces and pads as the device does

Status: built 2026-11-25 (branch feat/053-surface-sim). How to write a
test with it: [docs/dev/SURFACE_TESTS.md](../dev/SURFACE_TESTS.md).

**Why.** On the bench the PMJ Black 1's Go and Back did nothing, a learned
hit fired but its lamp stayed dark, and stray lamps lit. Every unit test
passed, because they called the overlay's slots and `ControlMap::handleInput()`
straight, past the plugin, the input patch, its buffer and its flush. The
simulator drives the entry point a USB device drives and checks what goes
back out to it.

## Decisions

1. **Enter where the device enters, run everything after it.** A MIDI board
   is the bytes it sends, decoded by the MIDI plugin's own
   `QLCMIDIProtocol::midiToInput()` (OMNI, as its line is for a profile whose
   channels carry the MIDI channel) and emitted as `MidiPlugin` emits them:
   `QLCIOPlugin::valueChanged(UINT_MAX, line, channel, value)`, on the I/O
   plugin stub dressed as a MIDI plugin (hardware lines named as the system
   names the device). A pad is the real HID plugin, its device listed by a
   `FakeHIDBackend` and opened into its virtual pad, fed whole USB input
   reports. Only the OS reader (the CoreMIDI callback, IOKit/GameController,
   evdev) is not run.
2. **The plugin's own tables read the reports.** An Xbox USB report goes
   through `GamepadLayout::parseGipReport()`; a DualSense or DualShock 4
   report is cut into its HID elements (usage page, usage, logical range,
   from the pad's report descriptor) and each goes through
   `GamepadLayout::mapElement()` / `scaleToByte()` / `hatToDpad()`, as
   `HIDOSXJoystick::processValue()` reads them; what changed leaves as its
   `emitChanges()` sends it. The simulator checks itself against the plugin:
   `SimPad::roleMismatches()` fails a test whose pad and plugin disagree.
3. **Record every output where it leaves.** MIDI out is the bytes
   `MidiPlugin::sendFeedBack()` would write (`feedbackToMidi()`, the profile's
   note-off rule, a control's own MIDI channel); `lamp()` is a control's last
   velocity. A pad's light, rumble and adaptive triggers are kept by its
   virtual pad (`HIDPlugin::takeVirtualFeedback()`, the one seam added).
4. **Controls by name, in the device's words** ("Go", "Master", "r2",
   "cross"); a control the device does not have fails at that line.
5. **Time is the app's.** `settle()` runs what a MasterTimer tick runs for
   input (`InputOutputMap::flushInputs()`) and the posted events. Holds and
   ramps are real waits (tens of ms), scaled by `setTimeScale()` (0: none).
6. **A script is JSON lines**, as the pad traces are (docs/dev/PAD_TRACES.md):
   `tap`, `press`, `release`, `set`, `ramp`, `chord`, `jitter`, `raw`,
   `wait`, `unplug`, `replug` (with a state dump). A capture is `raw` lines
   with their times; `tools/surface-capture.py` records one on the bench.
7. **Self-validation is table driven.** One data row per profile file in
   `resources/inputprofiles`, so a new profile is tested by being added; the
   PMJ overlay's table is checked against its profile and every control and
   lamp of it driven.
8. **A known bug is a QEXPECT_FAIL naming the branch that fixes it**, so the
   test flips (XPASS fails) the day that branch lands and the mark comes out.
9. **Header only, one CMake line** (`qlc_surfacesim(<test>)`), in
   `engine/test/common/surfacesim/`: engine and UI tests share it.

## The input paths (survey, 2026-11-25)

| Path | Where it enters | Simulated | Notes |
|---|---|---|---|
| MIDI (CoreMIDI, ALSA, WinMM) | `plugins/midi`: the OS reader calls `midiToInput()`, `MidiPlugin::slotValueChanged()` emits | yes: bytes → `midiToInput()` → `valueChanged()` | out: `MidiPlugin::sendFeedBack()` → `feedbackToMidi()`; SysEx and MTC not yet |
| HID game pads | `plugins/hid`: macOS GameController / IOKit elements / GIP report, Linux evdev, Windows own order | yes: USB report → `GamepadLayout` → virtual pad | DualSense, DualShock 4, Xbox (GIP); Linux evdev codes (`mapLinuxButton`) not yet |
| HID DMX interfaces | `hiddmxdevice` (FX5, Nodle) | no | output devices, not surfaces |
| Pad roles in the app | `PadRun`, `ProgrammerController` (light, rumble, triggers), `ControllerJobs`, `GamepadRole` | behind the HID path | |
| Steam Deck / phone page | `webaccess` (`WebAccessGamepad`, `webaccessrun`): browser Gamepad API → roles over the socket | no | its own tests (webaccess); a socket-level sim is next |
| OSC, Companion | `plugins/osc` (UDP, hashed channels); Companion's HTTP routes into `ShowControl` | profile rows at the plugin signal | packets not simulated |
| Keyboard | `ControlMap::KeyboardUniverse` keys | no | QTest key events already reach it |
| Profiles | 47 `.qxi` (MIDI, HID, OSC, Enttec); `.qxhid` for the HID plugin | every one, a row each | |
| Overlays | `ui/src/pmjoverlay` on `ControlSurfaceEngine` | the PMJ, its whole table | the only overlay |

## Self-validation (what every profile row checks)

* MIDI: each control's message, as the board sends it, decodes to that
  control's channel (a profile naming a channel no message arrives on is a
  bug), reaches `InputOutputMap::inputValueChanged()` on its universe, and
  its release does too; each control with a `<Feedback>` gets one MIDI
  message for its upper and lower level, addressing that same control.
* HID, OSC, Enttec: each channel, at the plugin's signal, reaches the app;
  a pad's `.qxi` has every control its `.qxhid` reads, a stick or trigger
  typed Slider and a button Button.
* The PMJ overlay (ui/test/pmjsim): its table is its profile's, channel and
  name; every control reaches its role in the surface engine or is in the
  documented free-key list; every `<Feedback>` control is sent a lamp, at
  a steady OpenDeck level, none is sent to a control without one, and a
  free key is dark.

## Device scenarios

`surfacescenarios.h`, each one call on any simulated device:
`unplugReplug(dev, dump)` (the system stops and starts listing it; the dump
is what the board sends the moment it is back), `sleepWake`, `momentary`,
`latchingPress`, `faderJitter`; `SimPad::drift()` (a stick resting off
centre); two identical boards (`SimMidiDevice(..., nth)`) on one name.

## In the gate

Three test programs: `engine/test/surfacesim`, `engine/test/padsim`,
`ui/test/pmjsim`; no device, offscreen, a few seconds together. A feature
that touches a surface adds a scenario to one of them (or its own test,
one CMake line). Left for later: the OS readers themselves, Linux evdev
pads, OSC packets, the web remote's socket, SysEx and MTC.
