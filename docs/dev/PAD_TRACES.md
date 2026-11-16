# Pad traces: record a real pad once, replay it in the gate

Real pads do what synthetic input never does: a DualSense clone on a cable
flips to "Xbox 360 Controller" when Create + Options is held, macOS lists a
pad twice, triggers rest above 0, sticks are noisy. The HID plugin can record
a session; the gate replays recordings through the real plugin into Pad Run
(`engine/test/padtraces`).

## Record (on dink, or any Mac)

Quit the app, then:

```sh
open -a /Applications/qlcconsole.app --env QLC_HID_RECORD=~/pad-traces --args -o ~/Documents/PadBench.qlcc
```

The show must have the pad **patched** (inputs are recorded as the plugin
delivers them to the engine, so an unpatched pad records its arrival only).
One file per session appears: `~/pad-traces/pads-<date-time>.jsonl`, written
as it happens and closed when you quit.

Press through a short script, one pad per session:

1. Each button once, slowly.
2. Both triggers, slowly, all the way down and back.
3. Both sticks round the full circle, then let go.
4. LB + RB to arm, LT + A.
5. The clone only: on a cable, hold Create + Options about 3 s, until it
   drops and comes back.
6. Unplug and replug it once.

Quit the app. The file is the whole record.

## Format

One JSON event per line; `t` is monotonic ms since the session began.

| `ev` | fields |
|---|---|
| `arrived` | `path`, `vid`, `pid` (hex), `name`, `serial`, `location`, `identity` (as ControllerJobs gives it), `axes`, `buttons` |
| `gone` | `path`, `name` |
| `input` | `path`, `ch` (channel index), `value` (as delivered to the engine) |
| `open_failed` | `path`, `name`, `vid`, `pid`, `why` (`stuck`, `nothing`, `given_up`) |

Lines starting `#` are comments. The replay also reads an optional
`open_ms` on `arrived` (how long its open takes; -1 never answers).

## Put one in the gate

1. Copy the file to `engine/test/padtraces/traces/<what-it-shows>.jsonl`
   (add a `#` line saying which pad, when, and what you pressed).
2. Point a test at it: copy a test in `padtraces_test.cpp`, or replace one of
   the synthetic traces with the same name. Play it with `PadTracePlayer`
   (`load`, `step`/`playUntil`/`playAll`), and assert the operator's rules
   (`engine/src/padrun.h`).
3. Look for orphan inputs (`orphanInputs()`): an input whose pad was not
   there means the trace and the plugin disagree.
4. `QLC_TEST_ONLY="engine/test/padtraces" ./check-all.sh --quick --remote auto --rev <SHA>`.

A trace that holds a name or serial you do not want in the repo: edit the
`name`/`serial` fields before committing.
