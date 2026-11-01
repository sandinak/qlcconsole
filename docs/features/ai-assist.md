# AI Assist

AI Assist answers questions about the show that is open, in plain words,
from the Show Tree's command line: *"? find address conflicts"*, *"? which
scenes does nothing use"*, *"? how many movers are patched"*. It works by
asking the console the same read-only questions you can type yourself, and
it shows you which ones, so you learn the command line as you go.

In this version it **only reads**. It never changes the show: no patching,
no cues, no values. It is off until you turn it on, it works with a model
running on this computer (no internet needed), and it is never there in
Operate.

## Why it reads, and you decide

A lighting console must do exactly what the operator tells it. A language
model can misread a request, so AI Assist never touches the show itself:
the model asks, the console answers, and only you act. Later versions will
propose changes as command-line text you review before anything happens;
every change will be one undo step.

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

1. hides the AI button and stops any question in flight (it says
   *Cancelled: the console went to Operate*);
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
mode*, and nothing is sent.

## Not in this version

Proposing changes (patch, scenes, cues) with a preview and one undo step,
importing universes and Art-Net setup from another show, and an MCP server
for outside tools are planned next. AI Assist will never touch playback,
timing or live output.
