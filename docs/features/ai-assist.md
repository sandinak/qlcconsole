# AI Assist

AI Assist answers questions about the show that is open, in plain words,
from the Show Tree's command line: *"? find address conflicts"*, *"? which
scenes does nothing use"*, *"? how many movers are patched"*. It works by
asking the console the same read-only questions you can type yourself, and
it shows you which ones, so you learn the command line as you go.

It can also **propose changes** to the Show Tree: *"? cue 5 towers blue at
50"*, *"? a new cue after this one called Bridge with the steps at 30"*.
Nothing changes until you apply the proposal, and then the whole of it is
one undo step. It is off until you turn it on, it works with a model
running on this computer (no internet needed), and it is never there in
Operate.

## Why it proposes, and you decide

A lighting console must do exactly what the operator tells it. A language
model can misread a request, so AI Assist never touches the show itself:
the model asks the console questions and proposes changes, the console
checks each proposed change and shows it to you as command-line text, and
only you apply it. One undo takes the whole change back.

## Turning it on

**Settings ▸ AI Assist** (this computer's, not the show's):

| Setting | What it does |
|---|---|
| **Turn on AI Assist** | Off by default. Off, nothing is made and nothing is sent anywhere. |
| **Provider** | **Ollama** (a model on this computer or your network), an **OpenAI-compatible server** (LM Studio, llama.cpp, vLLM and most local servers) or **Anthropic** (cloud). |
| **Endpoint** | Where the model answers. Ollama on this computer: `http://localhost:11434`. |
| **Model** | The model's name, as the server knows it. **Test connection** lists the ones it has. |
| **API key** | Cloud providers only. Kept in the macOS keychain, never in the settings. On other systems set `QLC_AI_API_KEY` in the environment. |
| **Send with a question** | **A summary of the show** (fixture names, definitions and addresses, groups, counts) or **Nothing up front**. Never the show file. |
| **Give up after** | Seconds to wait for an answer (60 by default). |

When the model runs anywhere but this computer, the page says so in amber:
your questions, and what the console answers them with (fixture names,
addresses, cues), leave this computer.

A good start is Ollama on the same Mac with a model that can call tools
(for example `qwen3.6:35b-a3b-mtp-q4_K_M`, the default).

## Asking

Open the Show Tree's command line (`:` from the tree) and type `?`, a
space and your question, then press Return. The **AI** button beside the
syntax button `?` starts one for you. You can keep working while it thinks;
**Esc** in the line cancels the question.

The answer appears in the pane above the line, with **Asked the console:**
and the commands it used (`conflicts`, `fixtures Spot`). Click one to put
it in the line: next time you can type it and skip the model.

`?` alone, and `?` with one word of the command line (`? spread`), are
still the command line's own help.

If the model can't be reached (Ollama not running, the wrong address), the
**AI** button greys out and its tooltip says why. Ask again or use **Test
connection** once it is back.

## Changes: proposed, then applied by you

Ask for a change the same way: `? cue 3 towers blue at 50`. The model
can't change anything itself. Its proposal appears in the pane above the
line as a checklist, each step the command-line text that does it and a
sentence saying what it does:

| | | |
|---|---|---|
| ☑ | `cue 3` | Go to cue 3 (made if it isn't there) |
| ☑ | `Towers colour Blue` | Towers: colour Blue |
| ☑ | `Towers dimmer 50` | Towers at 50% |

**Apply · Discard**

- **Enter** (on the empty line) or **Apply** makes the ticked steps, in
  order, as **one undo step** named *AI Assist: …*. Undo (⌘Z, or type
  `undo`) takes all of it back. The tree selects where the change left
  off, as a typed command does.
- Click a step's ☑ to leave it out (☐); click again to put it back.
- **Esc** or **Discard** drops the proposal. Typing anything else in the
  line, or asking a new question, drops it too. Nothing has changed.
- A single step also goes straight into the line, as you could have typed
  it: Enter runs it.

Each step runs where the one before left the selection, so `cue 3` then
`Towers colour Blue` sets the colour in cue 3. Before anything runs, every
step is checked: a value out of range, a word the step doesn't take or a
function the show doesn't have goes back to the model, and nothing is
proposed. If a step still can't be done when you apply (a group the show
doesn't have, no cue selected), the message says which step and why, and
**nothing** is applied: no part of the change is left behind.

What it can propose (each is a word of the command line; see `help`):

| Change | The command line it becomes |
|---|---|
| Go to or make a cue | `cue 3 "Bridge" block` |
| A new cue after the selected one | `cue + "Outro"` |
| Go to or make a cue list | `list "Encore"` |
| Looks on groups or fixtures | `Towers colour Blue dimmer 45`, `Towers, Steps gobo none` |
| A look for the whole cue | `cue look spread +5` |
| Intensity | `Movers dimmer 50` |
| The cue's mode | `block`, `track`, `cueonly`, `default` |
| Release on a layer | `release Towers` |
| Skip or put back cues | `skip 2, 4`, `unskip 2` |
| Record as new cue | `record selected` |
| Name the selected layer or cue | `name "Hero"` |
| Duplicate the cue | `dup` |
| Include a cue | `include 0.0.3` |
| A group set | `set "All" Towers Steps` |
| Play a function as a layer | `function Haze` |

Palettes made, renamed or deleted, patching and configuration import are
not changes it can propose yet; a colour or level it sets reuses the
palette that plays exactly that, or makes one, as the command line does.

## The questions it can ask

These are command-line words too. Each one changes nothing:

| Type | Answers |
|---|---|
| `fixtures [text]` | The fixtures: name, definition and mode, universe and addresses. |
| `groups` | The fixture groups, with their folder and size. |
| `functions [type] [text]` | Scenes, chasers, EFX, collections... by folder. |
| `describe <function>` | One function: what is in it and what uses it. |
| `cues [production]` | The cue sheet: every cue in running order. |
| `why <fixture>` | What is setting that fixture right now (see *Why is this on?* in [Troubleshooting](troubleshooting.md)). |
| `conflicts` | Fixtures on the same channels, or past the end of the universe (Show Review's PAT-01 and PAT-02). |
| `unused` | Functions nothing plays: no chaser, collection, cue, Virtual Console widget or start-up uses them. |
| `review [warning]` | Show Review's open findings, worst first (see [Show Review](show-review.md)). |
| `summary` | A short summary of the show. |

A group or fixture with one of these names keeps it: `summary colour Red`
still works on a group called Summary.

## Operate

Operate carries no AI at all. Going to Operate:

1. hides the AI button, stops any question in flight (it says
   *Cancelled: the console went to Operate*) and drops a proposal waiting
   to be applied: nothing of it is made;
2. closes every connection AI Assist had open;
3. if the model is an Ollama on this computer that answered a question this
   session, asks Ollama to unload it, so it gives back its memory;
4. checks that nothing of AI Assist is left running.

None of this delays the switch: Operate is immediate, always. If the check
finds something still running, or the model still loaded, the footer shows
**AI Assist resources still active in Operate mode** with the detail. It
stays until you go back to Design. It is a warning about memory, not a
reason to stop the show.

A question typed in Operate gets *AI Assist is unavailable in Operate
mode*, and nothing is sent. No change is ever applied in Operate.

## Bring in a setup from another show

**File ▸ Import ▸ Universes and I/O Setup (.qxw)…** takes the wiring of
another show into this one. It needs no AI: AI Assist can read the file and
propose the same import, but the form and the command line do it on their
own.

What it takes, for the universes you tick:

- **Universe names** (and passthrough).
- **Outputs**: each patch with its plugin, line and plugin settings
  (Art-Net or sACN address and universe, transmit mode).
- **Inputs**, with the input profile they use.
- **Feedback**.

It does not take fixtures, groups or palettes: **File ▸ Import ▸ Workspace**
does, and the form says how many the file has.

**Mapping.** Each file universe goes on a universe here (file universe 3 on
universe 1). *Network addresses* swaps an address wherever it appears
(`2.0.0.50 = 2.0.0.60`). *Plugins* swaps a plugin the file used for one you
have.

**Choices it will not guess.** If a universe here already has a setup (a
name of its own, or any patch in what you are importing), choose to
**skip** it (keep yours), **replace** it, or **renumber** (the file's goes
on a new universe at the end). If the file uses a plugin that isn't
installed here (a USB interface that isn't plugged in), choose to **skip**
those patches or **keep** them offline, or map the plugin to one you have.
Until you choose, **Import** stays grey and the preview says why. If
anything is in the way, nothing at all is imported.

**Networks.** A patch keeps the network it needs (its address and
network) exactly as the file has it, and waits, kept, until that network is
there, as a show opened away from its network does. So you can import at
your desk. The preview warns, and never stops you:

- *not currently routable* (expected when the show network isn't
  connected);
- *would leave by the DEFAULT route*: the address is only reachable through
  the house or Wi-Fi gateway, which usually means the show network card is
  down and output would go out the wrong network.

The preview shows each universe: what it has now and what it will have.
**Import** makes it as one undo step (Ctrl+Z puts the universes back
exactly; Redo again). Design mode only.

On the command line:

    config evo26.qxw
    import config evo26.qxw universes 2 map 2=1 address 2.0.0.50=2.0.0.60 missing keep
    import config evo26.qxw universes 2 map 2=1 address 2.0.0.50=2.0.0.60 missing keep apply

`config` lists the file's universes and patches. `import config …` shows
the preview; add `apply` to make it. Other words: `sections
universes,outputs,inputs,feedback`, `plugin Old=New`, `collision
skip|replace|renumber`.

With AI Assist on, `? take universe 2 and the Art-Net setup from
evo26.qxw, put it on 1` makes the assistant read the file and propose the
import as a step in the checklist; it is applied, like any step, only when
you confirm.

## Not in this version

An MCP server for outside tools is planned next. AI Assist will never touch
playback, timing or live output.
