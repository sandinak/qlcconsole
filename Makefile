# qlcconsole -- shortcuts for the macOS build. The real work is in the
# scripts each target calls; see docs/dev/RELEASE.md and CLAUDE.md ("Build & run").
#
#   make test      build what's checked out -> ~/Applications/qlcconsole-dev.app
#   make rc HOSTS= a signed test build installed on our Macs, not notarized
#   make dmg       signed + notarized universal DMG in dist/ (local only)
#   make beta      dmg + PUBLIC GitHub pre-release      (platforms/macos/publish.sh)
#   make release   dmg + PUBLIC GitHub release          (platforms/macos/publish.sh)

SHELL := /bin/bash
.DEFAULT_GOAL := help

DEV_APP ?= $(HOME)/Applications/qlcconsole-dev.app
export DEV_APP

.PHONY: help test run check quick strict pool pool-status rc install-test ux-walk perf perf-record perf-history-commit translations dmg release-check beta release attach publish

help:
	@v=$$(cat VERSION); \
	echo "qlcconsole make targets (macOS). VERSION is $$v."; \
	echo ""; \
	echo "Local only -- nothing leaves this Mac:"; \
	echo "  make test       Build the checked-out tree (incremental, build-dev/, Homebrew Qt)"; \
	echo "                  and install it as $(DEV_APP)"; \
	echo "                  -- ad-hoc signed, no DMG, for this Mac only. Opens from Finder or"; \
	echo "                  Spotlight (\"qlcconsole (dev)\"); from a terminal:"; \
	echo "                    \"$(DEV_APP)/Contents/MacOS/qlcconsole\" -o show.qlcc"; \
	echo "                  NOTE: despite the name it runs NO unit tests -- that's 'make check'."; \
	echo "  make run SHOW=path/show.qlcc"; \
	echo "                  'make test', then launch the dev app with -o SHOW (SHOW optional)."; \
	echo "  make check      The unit-test gate: ./check-all.sh (Qt6, Qt6-Release, Qt6-Werror),"; \
	echo "                  built together and tested concurrently; ends with gate=N."; \
	echo "  make quick      Iterate: ./check-all.sh --quick -- one leg, Debug with the -Werror set"; \
	echo "                  (build-qt6-werror/),"; \
	echo "                  testing just the test dirs your change touches since origin/main"; \
	echo "                  (its own tests, tests that #include a changed header). Not the gate."; \
	echo "  make strict     ./check-all.sh --strict: quick plus the Release leg (the touched tests"; \
	echo "                  at -O2 too). Before a branch is ready, and on a merged train."; \
	echo "  make pool       The full gate of a committed REV (default HEAD) on whichever Mac"; \
	echo "                  of platforms/macos/gate-hosts.conf is idle (HOST=name to pick);"; \
	echo "                  ends with gate=N host=... qt=... rev=...  make pool-status: who's free."; \
	echo "  make ux-walk [SHOW=a.qlcc] [UNITS=feet|mm|feet,mm] [OUT=dir] [HOSTS=brm,dink|auto] [NOLOCAL=1] [FLOWS=1]"; \
	echo "                  The UX walker (docs/dev/UX_WALKER.md): every tab, menu, dialog and"; \
	echo "                  right-click of real shows, offscreen, recorded and checked against"; \
	echo "                  docs/dev/ux_rules.json, plus the core flows (docs/dev/ux_flows.json;"; \
	echo "                  FLOWS=1: only those); OUT/report.html. Report-only, not the gate."; \
	echo "  make perf       The performance guard (tools/perf/guard.py, ~9 min): a Release engine"; \
	echo "                  (build-perf/) under the stress harness at fixed loads + the hot-path"; \
	echo "                  CPU costs, against this machine's baseline in docs/perf-data/;"; \
	echo "                  ends with perf=0 (within tolerance) or perf=1 (a regression)."; \
	echo "  make perf-history-commit  Commit docs/perf-data/history.csv (the nightly's rows merged in); never pushes."; \
	echo "  make perf-record  Record or rebase this machine's baseline (3 runs, ~27 min);"; \
	echo "                  commit it with the reason. HOST_CLASS=name names the machine."; \
	echo "                  PERF_ARGS=--points=gui,web adds only those points."; \
	echo "  make translations  Refresh every .ts catalogue from the sources (lupdate,"; \
	echo "                  ./translate.sh update). Translates nothing; run it every release."; \
	echo "  make rc HOSTS=\"dink kronk local\" [ARCH=native]"; \
	echo "                  A TEST build of a committed HEAD that holds origin/main: built"; \
	echo "                  (no DMG), Developer ID signed, NOT notarized, installed as"; \
	echo "                  /Applications/qlcconsole.app on each host over ssh (old -> .prev)."; \
	echo "                  For testing on our own Macs; publish nothing from it."; \
	echo "  make install-test HOSTS=\"...\"  install ~/qlcconsole.app as it is on those hosts."; \
	echo "  make dmg        The signed release DMG of a committed HEAD: universal Release build"; \
	echo "                  (build-universal/), Developer ID sign, notarize (qlc-notary), staple;"; \
	echo "                  dist/qlcconsole-$$v-universal-<date>-<rev>.dmg + dist/SHA256SUMS."; \
	echo "                  REPLACES ~/qlcconsole.app (old one -> ~/qlcconsole.app.prev);"; \
	echo "                  /Applications is never touched. Tags and publishes nothing."; \
	echo "                  Ends with 'make release-check' on the new DMG (RELEASE_CHECK=0 skips)."; \
	echo "  make dmg ARCH=native  The same for this Apple silicon Mac only: arm64, no x86_64 slice,"; \
	echo "                  dist/qlcconsole-$$v-arm64-native-<date>-<rev>.dmg. Signed and notarized,"; \
	echo "                  for installing on our own Macs; publish.sh refuses it."; \
	echo "  make release-check [DMG=dist/...] [HOST=ssh-host]"; \
	echo "                  platforms/macos/release-check.sh on the newest dist/*.dmg: signature,"; \
	echo "                  notarization, Gatekeeper, SHA256SUMS, universal + relocatable Mach-O,"; \
	echo "                  plugins, Info.plist and file types, resources, headless runs. Never"; \
	echo "                  opens a window. HOST= runs it on another (clean) Mac; WAIVE=id,id"; \
	echo "                  reports known failures as TODO. Windows/Linux: docs/dev/RELEASE.md."; \
	echo ""; \
	echo "PUBLIC -- push tag v$$v to origin and publish on GitHub Releases, where the in-app"; \
	echo "updater offers it to users. Each shows its plan and asks you to type 'publish':"; \
	echo "  make beta       test gate + 'make dmg' + tag v$$v + a GitHub PRE-RELEASE"; \
	echo "                  \"Show Tree $$v (beta)\" (Pre-release channel) with the DMG and"; \
	echo "                  SHA256SUMS, downloaded back and checked, then the URL."; \
	echo "  make release    the same as a FULL release \"qlcconsole $$v\" (Stable channel:"; \
	echo "                  offered to every user)."; \
	echo "      DRYRUN=1      show the checks, plan and release notes; build/publish nothing"; \
	echo "      SKIP_GATE=1   skip ./check-all.sh (normally run on a throwaway archive copy)"; \
	echo "      TITLE=\"...\"   another release name"; \
	echo "      DMG=dist/...  publish that already-built DMG of HEAD (from 'make dmg')"; \
	echo "      REPLACE=1     v$$v exists: delete its tag and release, publish again"; \
	echo "                    (asks you to type 'replace v$$v' as well)"; \
	echo "  make attach FILES=\"dist/qlcconsole-$$v-win64-setup.exe dist/qlcconsole-$$v-x86_64.AppImage\""; \
	echo "                  Add CI's files to the published v$$v release and re-upload its"; \
	echo "                  SHA256SUMS with their lines (asks you to type 'attach')."

test:
	@platforms/macos/dev-app.sh

run:
	@platforms/macos/dev-app.sh --run $(if $(SHOW),-o "$(abspath $(SHOW))")

check:
	./check-all.sh

quick:
	./check-all.sh --quick

strict:
	./check-all.sh --strict

pool:
	./check-all.sh --remote $(or $(HOST),auto) $(if $(REV),--rev $(REV))

pool-status:
	@platforms/macos/gate-pool.sh --status

# The UX walker (docs/dev/UX_WALKER.md). Opt-in, never in the gate.
UNITS ?= feet,mm
OUT ?= uxwalk-out
UXWALK_SHOWS ?= $(or $(wildcard test-workspaces/DanceHysteria-0.6.qlcc test-workspaces/Office.qlcc),docs/course/shows/lesson-3-9-finish.qlcc)
UXWALK_JOBS ?= $(shell sysctl -n hw.perflevel0.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)
ux-walk:
	@set -e; if [ -z "$(NOLOCAL)" ]; then \
	    cmake -S . -B build-uxwalk -DQLC_UXWALK=ON $(UXWALK_CMAKE) > /dev/null; \
	    nice cmake --build build-uxwalk -j$(UXWALK_JOBS) --target uxwalk; \
	fi; \
	python3 tools/uxwalk/run.py $(if $(FLOWS),--flows-only,$(foreach s,$(or $(SHOW),$(UXWALK_SHOWS)),--show $(s))) --units $(UNITS) \
	    --out $(OUT) $(if $(HOSTS),--hosts $(HOSTS)) $(if $(NOLOCAL),--no-local) $(if $(REV),--rev $(REV))

perf:
	python3 tools/perf/guard.py $(if $(HOST_CLASS),--host-class $(HOST_CLASS)) $(PERF_ARGS)

# decided (can change): history rows are committed by this step, not by the
# guard. It merges the nightly's file (PERF_HISTORY_FROM, default
# ~/git/perf-nightly/history.csv) into docs/perf-data/history.csv and commits
# that one file; it never pushes.
perf-history-commit:
	python3 tools/perf/history.py commit $(if $(PERF_HISTORY_FROM),--from $(PERF_HISTORY_FROM))

perf-record:
	python3 tools/perf/guard.py --record $(if $(HOST_CLASS),--host-class $(HOST_CLASS)) $(PERF_ARGS)

# Refresh every .ts catalogue from the sources (lupdate). Translates nothing.
translations:
	./translate.sh update

dmg:
	@QLC_ARCH="$(ARCH)" platforms/macos/make-release.sh

rc:
	@[ -n "$(HOSTS)" ] || { echo 'usage: make rc HOSTS="dink kronk local" [ARCH=native]'; exit 1; }
	@QLC_ARCH="$(ARCH)" platforms/macos/make-rc.sh $(HOSTS)

install-test:
	@[ -n "$(HOSTS)" ] || { echo 'usage: make install-test HOSTS="dink local"'; exit 1; }
	@platforms/macos/install-test-app.sh $(HOSTS)

release-check:
	@dmg="$(DMG)"; [ -n "$$dmg" ] || dmg=$$(ls -t dist/*.dmg 2>/dev/null | head -1); \
	[ -n "$$dmg" ] || { echo "no dist/*.dmg -- 'make dmg' first, or DMG=path"; exit 1; }; \
	platforms/macos/release-check.sh "$$dmg" $(if $(HOST),--host $(HOST)) $(if $(WAIVE),--waive $(WAIVE))

beta:
	@platforms/macos/publish.sh beta

release:
	@platforms/macos/publish.sh release

attach:
	@[ -n "$(FILES)" ] || { echo 'usage: make attach FILES="file ..."'; exit 1; }
	@platforms/macos/publish.sh attach $(FILES)

publish:
	@echo "'make publish' is gone: 'make beta' (pre-release) or 'make release' (full);"; \
	echo "'make dmg' builds the DMG without publishing. See 'make help'."; exit 1
