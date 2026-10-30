# qlcconsole — AI Assist (design + implementation handoff)

Status: proposed for next release. Owner: sandinak. Implementer: Claude Code.
Read DONE.md / TODO.md first; follow existing conventions there for status tracking.

## 1. Goal

Let an operator type a natural-language request inside qlcconsole ("movers 1–6 deep blue at 50%",
"patch 12 Rogue R2 Spot from 1.1", "find address conflicts") and get **proposed console commands**
they can review and execute. Optional, off by default, works fully offline with a local model.

## 2. Hard constraints

1. **Design mode only.** AI Assist exists only in Design mode. In Operate mode it is fully torn
   down — no requests, no threads, no sockets, no model held in memory (see §5a). Playback/output
   must never depend on the model. No AI call may block MasterTimer, universe threads, or the GUI
   thread (all network I/O async).
2. **Model proposes, console executes.** The model never writes `.qxw` XML or touches engine
   objects directly. It emits operations; the console validates and executes them.
3. **Human confirms.** Every proposed change is shown before execution. Apply = one undo group.
4. **Off by default.** When disabled: no network calls, no UI chrome beyond the settings toggle.
5. **Offline-capable.** Must work with a local Ollama endpoint and no internet.
6. **Separable.** All code under a dedicated module (`ai/`), behind CMake option
   `QLC_AI_ASSIST` (default OFF upstream-style; ON in our release build). Core engine gets no AI
   dependencies — keeps upstream (QLC+ v5) contribution paths clean.

## 3. Relationship to the command line (CLI)

AI Assist **augments the console command line; it does not replace it.**

- The command line is the deterministic, fast operator interface (Eos-style syntax).
- AI Assist translates intent → command-line text (plus structured ops the grammar doesn't cover).
- Output appears *in the command line* for review; Enter executes, Esc discards.
- Operators learn the syntax by seeing what the assistant generates.

**Dependency:** AI Assist Phase 1 requires the command-line grammar/parser. Check DONE.md/TODO.md
for current CLI status. If the CLI is not implemented, build the Operation Registry (§4) and CLI
first; AI Assist sits on top of both.

## 4. Architecture — one Operation Registry, three front ends

```
             ┌──────────────── Operation Registry ────────────────┐
             │ name · JSON schema · validate() · execute() ·       │
             │ cliSyntax() · describe()                            │
             └───────▲───────────────▲───────────────────▲─────────┘
                     │               │                   │
             Command-line parser   AI Assist panel     MCP server (Phase 3)
             (text → ops)          (model tool calls    (external agents:
                                    → ops → preview)     Claude Code, local)
```

- Each operation: unique name, JSON schema for args, `validate()` (address overlap, mode
  channel count, missing IDs, range), `execute()` via existing engine APIs inside the undo
  system, `cliSyntax()` to render the equivalent command-line text, `describe()` for preview.
- The CLI parser, the assistant, and the MCP server are thin front ends over the same registry.
  No operation logic lives in a front end.

### Initial operation set
Selection/values: `select_fixtures`, `set_intensity`, `set_capability` (resolve by capability
name via `.qxf` — color wheel vs CMY vs RGB), `apply_palette`, `record_cue`, `update_cue`.
Authoring: `patch_fixture(def, mode, universe, address, count, spacing)`, `create_scene`,
`create_chaser(steps, timing)`, `create_group`.
Read-only: `list_fixtures`, `list_functions`, `describe_function`, `find_conflicts`,
`find_unused_functions`, `show_summary`.

## 5. Settings (QSettings, group `ai/`)

| Key | Default | Notes |
|---|---|---|
| `ai/enabled` | `false` | Master toggle |
| `ai/provider` | `ollama` | `ollama` \| `openai-compatible` \| `anthropic` |
| `ai/endpoint` | `http://localhost:11434` | Base URL |
| `ai/model` | `qwen3.6:35b-a3b-mtp-q4_K_M` | Free text; "Test connection" lists models |
| `ai/sendShowContext` | `summary` | `none` \| `summary`; never the raw `.qxw` |
| `ai/timeoutSec` | `60` | Per request |

- API keys (cloud providers only) go in the OS keychain (QtKeychain or platform equivalent),
  **never** in QSettings/plist.
- Settings UI: one page with toggle, provider, endpoint, model, "Test connection", and a clear
  note when a cloud provider is selected that show context leaves the machine.

## 5a. Mode gating (Design vs Operate)

AI Assist is a Design-mode tool. Operate mode must carry zero AI load.

**Entering Design:** AI subsystem may initialize lazily on first use (if `ai/enabled`).

**Entering Operate — teardown sequence (synchronous from the mode-switch handler's view, but
must never block or delay the switch itself):**
1. Disable assistant entry points (command-line `?` prefix, panel, shortcuts).
2. Abort all in-flight requests; destroy the network manager and any worker threads.
3. If provider is a **local** Ollama on this machine: request model unload
   (`POST /api/generate {"model": <model>, "keep_alive": 0}`) so the model releases RAM/GPU.
4. Post-teardown audit: verify no AI threads/sockets/timers remain alive; for local Ollama, query
   `GET /api/ps` and confirm the model is no longer resident.

**If the audit fails** (anything still alive, or the model still resident):
- Force-terminate remaining AI objects.
- Raise a **visible error**: status-bar indicator + log entry
  ("AI Assist resources still active in Operate mode: <detail>"). It stays until resolved.
- **Never block or roll back the switch to Operate.** The operator must always be able to go
  live; the error is a warning about resource use, not a gate.

**In Operate:** any attempt to invoke the assistant (e.g. `?` in the command line) returns an
error message ("AI Assist is unavailable in Operate mode") and makes no network call.
A runtime guard (debug assert + release-mode error log) fires if any AI code path executes while
in Operate.

**Constraint to flag:** step 3 only works for a local Ollama the console can reach. If the model
runs on another machine, nothing on the show machine holds it; skip the unload, still audit
local threads/sockets.

## 5b. Configuration import ("map config from an existing file")

Deterministic operation in the registry (`import_config`), available **with or without AI**
(File → Import Configuration…, the CLI, and the assistant). Design mode only.

**Sources/sections** (user selects any subset from a source `.qxw`):
- Universes (names, IDs, passthrough)
- I/O patches: outputs, inputs, feedback — plugin, line, and per-line plugin parameters
  (Art-Net/sACN target IP, universe/net/subnet, transmit mode)
- Input profiles and their assignments
- Optionally: fixtures + patch, fixture groups, palettes

**Mapping:** user (or the assistant) supplies a mapping before apply, e.g. source universe 3 →
target universe 1, Art-Net destination 2.0.0.50 → 2.0.0.60.

**Network endpoints:** qlcconsole binds Art-Net/sACN by **network route requirement**, not by
local interface, so imported configs carry no machine-specific NIC bindings. Import copies the
route requirements as-is. Route checks are **warnings, not blockers** — designing off-network is
normal:
- **No route** to a destination/subnet on this machine → warn ("not currently routable — expected
  if the show network isn't connected").
- **Route resolves via the default route** rather than a specific/connected route → warn
  prominently. Art-Net/sACN for a show subnet (e.g. 2.x.x.x broadcast) leaving via the default
  route usually means the show NIC is down and output would go out the house/Wi-Fi network.
- Re-evaluate the same checks on switch to Operate and on route-table change (existing engine
  behavior permitting); never block the mode switch.

**Validation before apply (all must pass, else nothing is applied):**
- **Plugin availability:** source plugin/line must exist here (e.g. a USB DMX interface that is
  not attached). Missing → listed; user chooses skip or remap.
- **Universe collisions** with existing target universes: merge (skip), replace, or renumber.
- **Address conflicts** when fixtures are imported.
- **Dangling references:** imported groups/palettes referencing fixtures not imported.
- Fixture definitions referenced by imported fixtures must exist in the target's definition
  path; missing definitions listed.

**Preview:** diff of current vs resulting config (universes, patch lines with IPs, fixtures).
**Apply:** single undo group. Report what was imported, skipped, and remapped.

**Assistant use:** "take the universes and Art-Net setup from evo26.qxw, put universe 3 on 1"
→ assistant reads the source summary, proposes an `import_config`
call with mapping → normal preview/confirm.

## 6. Provider layer

- Async `QNetworkAccessManager`; cancellable; timeout from settings.
- Two adapters: OpenAI-compatible chat completions with tools (covers Ollama and most local
  servers) and Anthropic Messages with tools.
- Tools exposed to the model = Operation Registry schemas. Model output that is not a valid
  tool call against a registered op is rejected and shown as plain text.
- Context per request: compact show summary (fixture list with defs/modes/addresses, groups,
  palettes, current selection, active cue list), size-capped. No full show file.

## 7. UX

- Command line: a prefix (`?` at start of line) or toggle key switches to assistant mode.
- Response: proposed command(s) rendered in the command line + one-line explanation.
  Multi-step plans render as a checklist with per-step include/exclude and "Apply".
- Apply → validate all → execute as **one undo group**. Any validation failure aborts the whole
  group with the reason shown; nothing partial.
- Busy state is non-blocking; operator can keep working; Esc cancels in-flight request.
- Disabled/unreachable provider: assistant entry greyed with tooltip; nothing else changes.
- Operate mode: assistant entry hidden; `?` returns the Operate-mode error (§5a).

## 8. Phases and acceptance criteria

**P0 — prerequisites**
- Operation Registry exists; CLI parser (if present) refactored onto it; existing behavior unchanged.
- Accept: existing tests pass; CLI commands route through registry; unit tests per operation.

**P1 — settings + provider + read-only assistant**
- Settings page, provider adapters, read-only ops only (`list_*`, `find_*`, `describe_*`).
- Accept: with Ollama on localhost, "find address conflicts" returns correct results on a test
  show; disabled = zero network traffic (verify); provider down = graceful greyed state.

P1 must also ship the full §5a mode gating:
- Accept: switching to Operate during an in-flight request aborts it, never delays the switch,
  and leaves no AI threads/sockets; with local Ollama, `/api/ps` shows the model unloaded;
  `?` in Operate returns the error with zero network traffic; a deliberately leaked AI object
  triggers the visible error.

**P2 — write ops with preview, confirm, undo**
- Selection/value and authoring ops; preview in command line; one-undo-group apply.
- Accept: golden tests (recorded model responses → expected ops) for ≥15 prompts; undo
  restores exact prior state.

**P2b — configuration import (§5b)**
- `import_config` op + File menu entry + CLI form + assistant tool.
- Accept: import universes and Art-Net patches from a source show; route requirements copied
  intact; off-network import succeeds with the no-route warning; destination resolving via the
  default route produces the prominent warning; preview correct, apply is one undo group;
  plugin-missing and universe-collision cases each produce the specified choice; works with AI
  disabled.

**P3 — MCP server over the registry**
- Standalone or in-process MCP server exposing the same ops (read-only by default, write behind
  a flag), usable from Claude Code and local agents against a show file.
- Accept: Claude Code can list fixtures and run `find_conflicts` on a `.qxw` via MCP.

## 9. Testing

- Unit tests per operation (validate/execute/undo/cliSyntax round-trip).
- Provider adapters tested against a mock HTTP server; **no live model in CI**.
- Golden prompt suite: prompt → recorded tool-call response → expected op list.
- Patch ops cross-checked against the OLA RDM/Art-Net testbed where relevant.
- Threading: assert no AI code path runs on MasterTimer/universe threads.
- Mode gating: automated test toggles Design↔Operate repeatedly with requests in flight; asserts
  teardown audit passes and switch latency is unaffected.
- Config import: fixture `.qxw` pairs covering no-route, default-route egress, missing plugin,
  universe collision, dangling group reference.

## 10. Out of scope (this release)

- Any AI influence on playback, timing, or live output.
- Training/fine-tuning models.
- Raw `.qxw` generation by the model.
- Audio/video-reactive features.
