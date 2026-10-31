# Run commands: the one GO path

Status: built 2026-10-31 (0.4, first item; STRATEGY.md section 4, ROI 40).
Code: `ShowControl::run()` (engine/src/showcontrol.*) and
`App::runCommand()` (ui/src/app.cpp). Tests: engine/test/showcontrol,
engine/test/controlmap, engine/test/focuscalls, ui/test/appviews
(`everyRouteTakesTheOneGoPath`, `aGoThatFindsNothingHoldsNoPreview`).

## Why

GO, Back, Stop and the rest reached the Show Tree runtime by several
routes. Some went through `App::runCommand()` (a hook the Run tab used),
and some called `ShowControl::execute()` directly (the Control Map, a
focus-call cue, OSC, MSC). Pad Run, the phone Run page and override hits
all need one place to add their rules, such as Teaching's "allow remote
GO". That place is now `App::runCommand()`, and every route reaches it the
same way.

## The shape

```
any route ──> ShowControl::run(request, route)
                 │  run handler set (the App), on ShowControl's thread
                 ├──> App::runCommand(request, route)   guards, hold, notices
                 │        └──> ShowControl::execute(request)
                 │  no handler (engine alone, a test), or another thread
                 └──> ShowControl::execute(request)
```

- **The command set** is `ShowControl::Command`: `Go, Stop, Pause, Resume,
  Load, AllOff, Reset, Back, SubGo, SubBack`. A `ShowControl::Request`
  carries one command, plus an optional cue, a list (by number) or a
  chaser.
- **The route** is `ShowControl::Route`, which says where a command came
  from: `RouteKey, RouteRunTab, RouteVcRunner, RoutePad, RouteControlMap,
  RouteFocusCall, RouteOsc, RouteMsc, RouteWeb, RouteTimecode, RouteOther`.
  `ShowControl::isConsoleRoute()` is true for Key, Run tab and Pad.
- **The one function** is `ShowControl::run(request, route)`. A route
  never calls `execute()` itself.
- **The one guard** is `App::runCommand(request, route)`, installed as
  the run handler when the App starts and removed when it closes. In
  order, it:
  1. **Redirects:** ⌘↩ while a focus-call cue is armed starts its calls
     and plays no GO (this was in the key handler before).
  2. **Refuses:** a `RouteWeb` command while the show's **Allow GO from
     the phone Run page** (`ShowOperate::remoteGo()`, Show Settings) is
     off. *decided: Operate's PIN and Teaching lock Design, not GO (can
     change).* The PIN is the page's to check, once per websocket session
     (`WebAccessRun`), since only it knows the session; it also checks the
     setting itself, so the page is as safe without the App.
  3. **Holds the previews (RC8):** if the command will put a cue on stage
     (`ShowControl::wouldTakeStage()`), it calls
     `ShowTreeController::takeStageForRun()` first and remembers what it
     held.
  4. **Executes:** `ShowControl::execute()`.
  5. **Speaks:** on console routes only. A GO in Design offers Operate
     (Q92); otherwise, if something was held, a notice says so. Every
     other route holds the previews quietly.

`ShowControl::execute()` still emits `aboutToTakeStage()` on the same
condition, so a command that skips the App (on another thread, or with no
App) still holds the previews, through a queued connection.

## The routes

| Route | Where | Commands | Speaks |
|---|---|---|---|
| Key | `App::m_cueGoAction` / Back / Sub GO / Sub Back (⌘↩, ⌘⇧↩, ⌥⌘↩, ⌥⌘⇧↩; Control ▸ GO…) | Go, Back, SubGo, SubBack on the main list | yes |
| Run tab | `CueRunner` Full: buttons, Space, a click (Load) or double-click (Go cue) in the list | all but AllOff and Reset | yes |
| VC Cue Runner | `CueRunner` Compact/List inside `VCCueRunner` (click, its key, its external input) | the same | no |
| Pad (Run role) | `PadRun::trigger()` (engine/src/padrun.*): LB held + A / B / Y / X; RT / LT pick the list | Go, Back, SubGo, Stop on the main or picked list | yes |
| Pad (profile action) | `ProgrammingManager::slotButtonAction("chaser_step_forward"/"back")` on a running Show Tree list, Tools role only | Go, Back | yes |
| Control Map | `ControlMap::goList()` (GO / Stop a list or cue) | Go, Stop | no |
| Focus call | `FocusCallCue` continuing its list when the calls are done | Go | no |
| OSC | `ShowControl::handleOsc()` (from `ShowControlListener`) | all | no |
| MSC | `ShowControl::handleSysEx()` (a patched MIDI input) | Go, Pause, Resume, Load, AllOff, Reset, Stop | no |
| Web | the phone Run page (`/run`, `WebAccessRun::command()`) | Go, Back, Stop, SubGo, SubBack on the main list | no |
| Timecode | `TimecodeCues` (a list set to follow timecode reaches a cue's time) | Go (by cue number and chaser) | no |

**Not routes:**
- **The footer's "Next:"** shows status only (Q74). A click brings the
  Run tab forward.
- **The Show Tree command line** has no playback words.
- **Stop all / panic** (⌘⇧Esc) stops the master timer itself. It is not
  a cue command, and it stays separate on purpose.
- **Two stock QLC+ paths step a chaser directly:** the VC Cue List (and
  the web access page that drives it) and a VC button's "next/previous
  step" (`Doc::stepCurrentChaser`). They are left as they were. They can
  be moved onto `run()` if a Show Tree list ever plays through them; the
  phone Run page is the natural time to do it.

## Behaviour

Every route behaves as before, with one deliberate exception. A GO, Back,
Sub GO or Sub Back that finds nothing to play (no main list, or no such
list or cue) no longer stops the previews or shows a notice from the keys.
Before, the App held the previews before it knew the command would fail.
*decided: holding only for a command that will play (can change).*

## Hot path

None of this is on the frame path. `run()` is called once per command,
and the handler is one `std::function` call. The MasterTimer, faders and
the Show Tree runtime are untouched.

## Next

- **Pad Run:** built (0.4): `PadRun` calls `run(r, RoutePad)`; see
  docs/features/show-control.md "GO from the game controller". Walking
  the show (Q192) is built on it: Menu enters a walking map
  (`PadRun::handleWalk()`) that has no GO; cue steps preview, never run.
- **Phone Run page:** built (`RouteWeb`; webaccess/src/webaccessrun.*;
  tests in webaccess/test and ui/test/appviews). The VC Cue List page of
  stock web access still steps its chaser directly.
- **Override hits** (built 2026-10-31): firing a hit stays a VC button's
  start, not a run command (decided, can change); ALL OFF, which lets go
  of them, comes through here.
