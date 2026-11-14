# Agent brief: the first lines

Paste this block at the top of every qlcconsole agent brief, fill in the
`<…>`, then write the task under it. Each rule is here because an agent broke
it at least once; a rule added later as a correction costs a rerun.
Background: [CLAUDE.md](../../CLAUDE.md) (Build & run, gate cadence, merging
a train) and [RELEASE.md](RELEASE.md#process-tools).

```text
RULES (read before anything else)

Where you work
- Your own clone, never the primary checkout (/Users/branson/git/qlcconsole):
    git clone /Users/branson/git/qlcconsole /Volumes/Ext/git/qlcconsole-<tag>   (else ~/git/)
    cd there; git remote set-url origin ssh://git@ssh.github.com:443/sandinak/qlcconsole.git
    tools/gitsync.sh fetch && git checkout -b <branch> origin/main
  If the clone or the fetch fails: STOP and report. Do not branch, commit or
  stash in the primary checkout.
- Fetch with tools/gitsync.sh fetch (port 443 when 22 fails). Never trust
  origin/main without fetching first.

Builds and tests: on the pool, never on ender
- Commit (a WIP commit is fine), push your branch, then from your clone:
    QLC_TEST_ONLY="<test dirs>" GATE_HOSTS=<hosts conf> ./check-all.sh --quick --remote auto --rev "$(git rev-parse HEAD)"
  --quick builds with the -Werror set; before you call the work ready run the
  same with --strict (adds the Release leg: -O2 finds what Debug hides). Your
  report quotes the --strict result line.
- On ender only `cmake --build build --target X -j2`. A hook blocks the rest;
  do not get around it: no scp of scripts or archives to pool hosts, no ssh
  builds there by hand. If the pool cannot do what you need, STOP and say
  what is missing.

Screenshots (any UI change)
- Tests save PNGs into $QLC_SHOT_DIR (QSKIP when it is unset). Get them with
    QLC_TEST_ONLY="ui/test/<dir>" ./check-all.sh --quick --shots <shots dir>/<before|after> --remote auto --rev <SHA>
  (before = the SHA you branched from, after = yours). Sizes 1024x640,
  1440x900, 1920x1080; layoutsanity_test stays green. Never hard-code a path
  in a test or copy files out of ~/gates. Never ask Branson to look at them:
  the ux-reviewer signs them off.

Shared files
- CHANGELOG.md: one line under ## [Unreleased] in the right ### list.
- docs/dev/TEST_MAP.md: new rows start `| Mnew |`. Do not pick a number;
  the train numbers them (tools/test-map-ids.py assign).
- Add a test function next to related ones, not only at the end of the file.

What "done" means
- A feature that can't act says so on the screen, not only by voice or in a
  log: test what the user sees (footer, label, enabled state), not just the
  model. Anything that needs a real pad, Deck or rig gets a numbered step in
  its hardware checklist (docs/features/*.md), with the expected result.
- Help (docs/features), a CHANGELOG line, tests in the gate.
- Every length through MeasurementUnits (units_test).

Never
- Push to main, merge, tag, publish (make beta/release/attach), or notarize
  to test (a test build is `make rc`, and only the lead runs it).
- Send DMX to Art-Net 0:0:0 (the office rig). Drive the real screen.
- Resolve a conflict with sed: tools/merge-check.py resolve, then check.
- In zsh, write "$SHA:refs/…" unbraced: always "${SHA}:refs/…".

Report: branch, head SHA, the --strict line, shots dir, what changed, what
is left open. Commit trailer: Co-Authored-By: <model line from the lead>.
```

## For the lead

- Plan where the agents' builds go before spawning them (`make
  pool-status`); two or three gating agents at once, not eight.
- Give each agent its own `<tag>`, branch and shots dir; nothing else to
  allocate (TEST_MAP ids are `Mnew`).
- A train: `tools/gitsync.sh fetch`, merge the branches (`--no-ff`),
  `tools/merge-check.py check`, `tools/test-map-ids.py assign` + commit,
  `--strict` on the merged SHA, the full gate, `tools/gitsync.sh push-main
  <SHA>`. See CLAUDE.md, "Merging a train".
- An RC for the bench: `make rc HOSTS="<bench Mac>"` the moment the train is
  green; `make dmg` (notarized) only for what gets published.
