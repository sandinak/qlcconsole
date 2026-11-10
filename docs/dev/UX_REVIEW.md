# AI UX review

Every UI branch's before/after screenshots are signed off by this review
before the branch merges
([UX-40](UX_RULES.md#ux-40-a-ui-change-shows-its-before-and-after)). Since
2026-11-09 it **replaces** Branson's own look: he used to catch the same
obvious things by hand (a metres label on a feet show, a button not in Title
Case, a dialog that runs off a laptop screen), and waiting on him blocked
merges. The reviewer reads the shots against his written rules
([UX_RULES.md](UX_RULES.md), machine copy [ux_rules.json](ux_rules.json)).
Never ask him to approve shots.

It does not replace a gate test or the walker's live-widget checks. It is
the `review` check in the rules: the one that reads pixels the way he does.

## The loop

1. **The author agent** changes the UI and takes before/after shots at
   1024x640, 1280x800 and 1920x1080 (offscreen: a test's `QLC_SHOT_DIR` or
   `LAYOUT_SANITY_SHOTS`) into `shots/<branch>/`.
2. **The screen walker** (tools/uxwalk, when it lands) opens every screen
   offscreen and writes a PNG, a JSON text record and report.html per
   screen; it asserts the `walker` rules on live widgets.
3. **The reviewer** runs over the branch's shots (and the walker's output):
   one `ux-reviewer` subagent per packet, all at once, then one merged
   report.
4. **The author fixes** what the report calls `blocker` or `fix` (or says
   in the hand-off why a finding is wrong), retakes the shots and reruns
   the review until it is clean or only nits and known follow-ups remain.
5. **Signed off:** clean, or only nits and known follow-ups, and the branch
   may go into a train. The report goes in the morning report; Branson is
   not asked to look.

## Running it

```sh
# 1. Packets: group the shots by screen, ~6-10 images each, with only the
#    rules each screen can show. --context says what the branch changed.
tools/uxreview/uxreview.py plan shots/fix-041-units --out ~/git/uxr/units \
    --context "fix-041-units=Every length follows Settings > Units" \
    [--diff /tmp/branch.diff]       # forbid_regex over the diff's added tr("...")

# 2. Review: one ux-reviewer per packet, in parallel (see below).

# 3. Merge: dedupe, then findings.json + report.md (thumbnails link the shots).
tools/uxreview/uxreview.py merge ~/git/uxr/units
```

`plan` reads before/after and the size from the file name in any of the
forms in use (`before-X-laptop`, `X-after-1024x640`, `greyrow-menu-after`);
a name with neither word is a current shot (the walker's). It also writes
`packets.json` (the packet list, which is also the Workflow script's args)
and `regex_hits.json`: every `forbid_regex` in ux_rules.json run over the
walker's text records and the diff, with no model. `merge` exits 1 and lists
any packet that has no findings.json, so a reviewer that died is never read
as "no findings".

**Step 2, from an orchestrator session** (the usual way): one Agent call per
packet, all in the same message, `subagent_type: "ux-reviewer"` (defined in
[.claude/agents/ux-reviewer.md](../../.claude/agents/ux-reviewer.md), Sonnet),
with the line `tools/uxreview/uxreview.py prompt RUN/pNN` prints. A session
started outside this repo doesn't know the agent type: use
`general-purpose`, `model: sonnet` and the same line (it tells the agent to
read ux-reviewer.md first). A session runs at most 20 subagents at once;
more packets than that go in a second message as the first ones finish.

**Or as a Workflow**: [tools/uxreview/review.workflow.js](../../tools/uxreview/review.workflow.js)
fans the packets out with `parallel()` (one `ux-reviewer` each) and runs the
merge. Pass the contents of `RUN/packets.json` as `args` (the object, not a
string). It is the better fit for a large set (the walker's whole app),
since the Workflow tool queues past its own concurrency cap and shows
progress per packet.

## The diff packet: the change itself

Some rules break before any screen shows it: a feature added where nobody
working will find it, a thing you build but can't try in Design, a help
page with "1.2 m". With `--diff`, `plan` adds one packet of kind `diff`
(no images, works without shots too):

```sh
git diff origin/main...HEAD > /tmp/branch.diff
tools/uxreview/uxreview.py plan --diff /tmp/branch.diff --out ~/git/uxr/br [shots dirs...]
```

It carries the rules a diff can break (`DIFF_RULES` in uxreview.py: UX-61
findable from where you work, UX-62 testable where it's built, UX-63 help
lengths, UX-64 read it from the device, UX-65 Auto shows its number, UX-67 to UX-75 (does what it says), UX-66
and the complexity bar, UX-44 flat surface, UX-23, UX-12, UX-17, UX-60,
UX-01/02; the course rules when the diff touches docs/course), the bar's
checks C-1 to C-8 from [ux_flows.json](ux_flows.json), the forbid_regex
hits on the diff (UX-63 reads added help lines, code spans left out), and
`touched_flows`: the core flows whose controls (objectNames) the diff adds,
removes or changes. The reviewer reads the diff and answers, with the
diff's own lines as evidence:

- **UX-61**: does a new feature, setting or mode have a control or a
  pointer where the work happens (the list, the cue, the fixture), or only
  on a page the person wouldn't be on?
- **UX-62**: can what it builds be tried in Design, from where it's built?
- **UX-64 / UX-65**: hand entry where the device could be read; an Auto
  that doesn't show its number.
- **The bar (UX-66, C-1..C-8)**: a new menu item or tab where a pointer or
  a row would do (C-4, UX-44); a word the course doesn't use (C-5); a step
  that needs the help page (C-6); a pro shortcut that replaced the clicks
  (C-8); a flow made longer.
- **touched_flows**: each one is a finding to re-walk (`make ux-walk
  FLOWS=1`) unless the diff's change to it is obviously safe (a nit).

## What the reviewer is told

- Judge the **after**; use the before only to mark a problem new
  (`regression: true`; a blocker when it breaks reading, use or meaning). A
  problem in both is an existing follow-up, never a blocker. One fixed by
  the branch is not a finding.
- **Quote** what it sees (`evidence` is the on-screen text exactly, or a
  precise place for a layout fault); if it can't read it, it doesn't
  report it. "No findings" is the common, good answer.
- The noise list: offscreen rendering, the shot tool's caption, crops vs
  a dialog running off a full screen, scrolling vs clipping, elided free
  text, test data, Color/Colour, questions in menus ("Why is this on?"),
  taste no rule states.
- Each finding: `rule, screen, subject, file, files, what, evidence,
  severity (blocker|fix|nit), confidence (high|medium|low), regression`.
  `subject` names the widget the same way wherever it appears; the merge
  folds one subject on many screens and sizes into one finding.

## Dry run, 2026-11-06 (the 0.4.1 branches' shots)

200 shots from six approved branches (units, cli, fxrows, fxfit, layout,
small) in 33 packets, Sonnet reviewers, two waves of parallel Agent calls
(the 20-subagent cap).

- **Time:** 2 min 10 s wall for all 33 packets, plan to merged report;
  each reviewer 10-18 s. A typical branch (12-18 shots, 2-3 packets) is
  about 20 s.
- **Cost:** about 55k tokens per packet (mostly the images); about 1.8M for
  the 200 shots, roughly 9k per screenshot.
- **First pass:** 27 findings (14 fix, 13 nit), 15 distinct problems once
  folded. 14 real, 1 noise. Real and worth a follow-up: "Show all effects"
  not Title Case in the effect picker (all three fxfit screens; fixed in
  source since by fix/041-small's button check); look names cut to "B…" /
  "F…" in Scene ▸ group looks at the minimum size; the Tower, Truss and
  Platform dialogs' right-hand buttons cut ("Cop…es…", "Mea…re…"), "On
  sides" checkboxes cut to "Fro Bac Sta Sta", the "No limit" spin box cut
  at its left, plot dimension labels drawn over "stage right", the truss
  Sock row's "…" button over its box; the layer name cut to "Tow…" in the
  Show Tree at 1024x640; the "+ Effect" button's menu arrow over its "t"
  (reported on 12 screens). None were regressions: every one was in the
  before shots too. The noise: "Why is this on?" flagged as not Title Case
  (it is a question, exempt by design).
- **Missed on the first pass:** the Tower and Truss dialogs run off the
  bottom of a 1024x640 screen, OK/Cancel out of sight. The reviewer read it
  as a crop.
- **Tuned and rerun** (6 packets): the question exemption and the
  crop-vs-full-screen rule went into the instructions, `subject` into the
  output. The noise finding went away, both off-screen dialogs were caught,
  and the "+ Effect" reports folded into one. 9 findings, all real.

Precision on this set: 14/15 distinct problems real on the first pass,
9/9 after tuning. Recall is not measured: nobody has a full list of what
these screens get wrong; the off-screen dialog miss says it is not 100%.
Run-to-run variance is real (one rerun dropped the minor "1' 4" under
stage right" overlap), so a clean report is evidence, not proof.

## Adding or changing a rule

1. Write it in both [UX_RULES.md](UX_RULES.md) and
   [ux_rules.json](ux_rules.json) (same ID, title, rule, checks, Right,
   Wrong). Add a `forbid_regex` when the wrong text has a shape; it must
   match the Wrong example and not the Right one.
2. `tools/uxreview/check_rules.py` keeps the two in step and checks every
   `test:X::Y` exists and every regex against its examples. The gate runs
   it before building (check-all.sh, next to check-win-macros.sh); run it by
   hand after an edit.
3. If a screenshot can show the rule broken, add its ID to `CORE_RULES` (any
   screen) or a `TOPIC_RULES` line (screens whose set, name or context
   matches) in tools/uxreview/uxreview.py, so packets carry it. If a diff
   can show it before any screen does, add it to `DIFF_RULES`.
4. When the reviewer gets it wrong in a way that repeats, add one line to
   the "What is NOT a finding" or "What counts" list in
   .claude/agents/ux-reviewer.md, with the example, and rerun the packet
   that showed it.
