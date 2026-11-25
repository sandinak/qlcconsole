# Writing a surface test

A surface test drives a control surface or a game pad the way the device
does -- its MIDI bytes, its USB input reports -- and checks what the app
sends back (lamps, MIDI out, the pad's light and rumble). Why and how it is
built: [SURFACE_SIMULATOR.md](../design/SURFACE_SIMULATOR.md). Never call an
overlay's slot or `ControlMap::handleInput()` to stand for a press: that is
what let the PMJ's dead Go pass every test.

## One line in CMake

```cmake
qlc_surfacesim(mytest_test)
```

That adds the simulator's headers (`engine/test/common/surfacesim/`), the
plugins' decoders it feeds through and the I/O plugin stub. A pad test also
loads the real HID plugin, as [engine/test/padsim/CMakeLists.txt](../../engine/test/padsim/CMakeLists.txt)
does (`add_dependencies(... hidplugin)`, `QLC_HID_PLUGIN_FILE`).

## A MIDI board

```cpp
#include "simmididevice.h"

SimMidiDevice pmj(doc, stub, "PMJ Black 1", 0, "PMJ Black 1",
                  QLC_TEST_RESOURCE_ROOT "inputprofiles/PMJ-Black-1.qxi");
QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));   // listed, input + feedback patched

pmj.tap("Go", 120);                      // note on 98 00 7f, 120 ms, note off
pmj.ramp("Master", 0, 127, 300);         // CC 0 on MIDI channel 9, a hand's move
pmj.raw(QByteArray::fromHex("980b7f"));  // bytes exactly as the board sends them
QCOMPARE(pmj.lamp("Go"), 127);           // the velocity the board was last sent
```

`stub` is the I/O plugin stub (load it as
[ui/test/pmjsim/pmjsim_test.cpp](../../ui/test/pmjsim/pmjsim_test.cpp) or
[engine/test/surfacesim/surfacesim_test.cpp](../../engine/test/surfacesim/surfacesim_test.cpp)
do). Controls are the profile's names; `"#32896"` is a raw channel. A
second identical board: `SimMidiDevice(..., nth = 1)`.

## A game pad

```cpp
#include "simpad.h"

SimPad pad(doc, hidPlugin, fakeBackend, SimPad::DualSense);
QVERIFY2(pad.plug(), qPrintable(pad.lastError()));
QVERIFY2(pad.roleMismatches().isEmpty(), "the simulator and the plugin disagree");
pad.chord({ "l1", "r1" });               // whole USB reports, one per change
pad.press("r2"); pad.release("r2");
QCOMPARE(pad.lastLight(), PadRun::armedColour().name());
```

Models: `DualSense`, `DualShock4` (report 0x01), `XboxUsb` (GIP 0x20).
Controls by role (`a`, `lb`, `rt`, `lx`, `dpad_up`) or the pad's own names
(`cross`, `l1`, `r2`, `options`). `pad.raw(report)` feeds a captured report.

## What hardware does

[surfacescenarios.h](../../engine/test/common/surfacesim/surfacescenarios.h):
`unplugReplug(&dev, {{"Favorites", 0}, {"Favorites", 127}})` (the dump is
what the board sends on reconnect), `sleepWake`, `latchingPress`,
`faderJitter`; `pad.drift("lx", 9)`. Set the show up, run one, assert the
show did what a person would expect -- usually nothing.

## Scripts and recordings

A script is JSON lines (format in
[surfacescript.h](../../engine/test/common/surfacesim/surfacescript.h)):

```json
{"ev":"tap","control":"Go","hold_ms":120}
{"ev":"ramp","control":"Master","from":0,"to":127,"ms":300}
{"ev":"replug","dump":[["Master",64],["Favorites",0]]}
```

`SurfaceScript s; s.load(file); s.play(&dev);` -- a failing line is named.

### Recording a device

On the bench, with qlcconsole quit:

```sh
tools/surface-capture.py list
tools/surface-capture.py midi --port "PMJ Black 1" -o pmj-go.jsonl
tools/surface-capture.py hid --vid 054c --pid 0ce6 -o dualsense-r2.jsonl
```

It writes `raw` lines with their times; put the file next to the test
(like [engine/test/surfacesim/captures/](../../engine/test/surfacesim/captures/))
and play it. A pad can also be recorded by the plugin itself
([PAD_TRACES.md](PAD_TRACES.md)): those traces are values after the
plugin's reading, these are the reports before it.

## A known bug

Pin it with the branch that fixes it, so the test flips when it lands:

```cpp
QEXPECT_FAIL("", "fix/053-pmj-run-buttons wires the PMJ's Go", Continue);
QVERIFY(list->isRunning());
```

When the fix merges the test XPASSes, which fails it: take the mark out.

## A new profile or overlay

A new `.qxi` in `resources/inputprofiles` is a new row of
`SurfaceSim_Test::everyProfileControlReachesTheApp` by being there. A new
overlay copies `PmjSim_Test`'s three table tests (its table is its
profile's; every control reaches its role or is a documented free key;
every lamp is sent at a valid level and none stray).
