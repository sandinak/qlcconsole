# qlcconsole -- shortcuts for the macOS build. The real work is in the
# scripts each target calls; see docs/dev/RELEASE.md and CLAUDE.md ("Build & run").
#
#   make test      build what's checked out -> ~/Applications/qlcconsole-dev.app
#   make dmg       signed + notarized universal DMG in dist/ (local only)
#   make beta      dmg + PUBLIC GitHub pre-release      (platforms/macos/publish.sh)
#   make release   dmg + PUBLIC GitHub release          (platforms/macos/publish.sh)

SHELL := /bin/bash
.DEFAULT_GOAL := help

DEV_APP ?= $(HOME)/Applications/qlcconsole-dev.app
export DEV_APP

.PHONY: help test run check quick pool pool-status dmg release-check beta release attach publish

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
	echo "  make quick      Iterate: ./check-all.sh --quick -- the Qt6 leg only (build-qt6/),"; \
	echo "                  testing just the test dirs your change touches since origin/main"; \
	echo "                  (its own tests, tests that #include a changed header). Not the gate."; \
	echo "  make pool       The full gate of a committed REV (default HEAD) on whichever Mac"; \
	echo "                  of platforms/macos/gate-hosts.conf is idle (HOST=name to pick);"; \
	echo "                  ends with gate=N host=... qt=... rev=...  make pool-status: who's free."; \
	echo "  make dmg        The signed release DMG of a committed HEAD: universal Release build"; \
	echo "                  (build-universal/), Developer ID sign, notarize (qlc-notary), staple;"; \
	echo "                  dist/qlcconsole-$$v-universal-<date>-<rev>.dmg + dist/SHA256SUMS."; \
	echo "                  REPLACES ~/qlcconsole.app (old one -> ~/qlcconsole.app.prev);"; \
	echo "                  /Applications is never touched. Tags and publishes nothing."; \
	echo "                  Ends with 'make release-check' on the new DMG (RELEASE_CHECK=0 skips)."; \
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

pool:
	./check-all.sh --remote $(or $(HOST),auto) $(if $(REV),--rev $(REV))

pool-status:
	@platforms/macos/gate-pool.sh --status

dmg:
	@platforms/macos/make-release.sh

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
