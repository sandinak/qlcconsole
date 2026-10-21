# qlcconsole -- shortcuts for the macOS build. The real work is in the
# scripts each target calls; see RELEASE.md and CLAUDE.md ("Build & run").
#
#   make test      build what's checked out -> ~/Applications/qlcconsole-dev.app
#   make release   signed + notarized universal DMG in dist/ (no publishing)

SHELL := /bin/bash
.DEFAULT_GOAL := help

DEV_APP ?= $(HOME)/Applications/qlcconsole-dev.app
export DEV_APP

.PHONY: help test run check release publish

help:
	@echo "qlcconsole make targets (macOS):"
	@echo ""
	@echo "  make test       Build the checked-out tree (incremental, build-dev/, Homebrew Qt)"
	@echo "                  and install it as $(DEV_APP)"
	@echo "                  -- ad-hoc signed, no DMG, for this Mac only. Opens from Finder or"
	@echo "                  Spotlight (\"qlcconsole (dev)\"); from a terminal:"
	@echo "                    \"$(DEV_APP)/Contents/MacOS/qlcconsole\" -o show.qlcc"
	@echo "                  NOTE: despite the name it runs NO unit tests -- that's 'make check'."
	@echo "  make run SHOW=path/show.qlcc"
	@echo "                  'make test', then launch the dev app with -o SHOW (SHOW optional)."
	@echo "  make check      The unit-test gate: ./check-all.sh (Qt6, Qt6-Release, Qt6-Werror)."
	@echo ""
	@echo "  make release    The full release DMG, from a clean checkout of HEAD:"
	@echo "                  universal Release build (build-universal/), Developer ID sign,"
	@echo "                  notarize (keychain profile qlc-notary) and staple; writes"
	@echo "                  dist/qlcconsole-<VERSION>-universal-<date>-<rev>.dmg + dist/SHA256SUMS."
	@echo "                  It also REPLACES ~/qlcconsole.app with the signed build (the old one"
	@echo "                  is kept as ~/qlcconsole.app.prev). /Applications is never touched."
	@echo "                  Does not tag or publish anything."
	@echo "  make publish    Explains, and after you confirm runs, ./release.sh: tag vVERSION"
	@echo "                  and create the public GitHub Release (it rebuilds; see RELEASE.md)."

test:
	@platforms/macos/dev-app.sh

run:
	@platforms/macos/dev-app.sh --run $(if $(SHOW),-o "$(abspath $(SHOW))")

check:
	./check-all.sh

release:
	@platforms/macos/make-release.sh

publish:
	@v=$$(cat VERSION); \
	echo "make publish runs ./release.sh, which will:"; \
	echo "  1. run the test gate (build/, --target check)"; \
	echo "  2. rebuild + sign + notarize the universal DMG (as make release; replaces ~/qlcconsole.app)"; \
	echo "  3. tag v$$v and PUSH the tag to origin"; \
	echo "  4. create a PUBLIC GitHub Release v$$v with the DMG and SHA256SUMS"; \
	echo "     (PRERELEASE=1 TITLE=\"...\" make publish for the Pre-release channel)"; \
	echo "Then attach CI's Windows/Linux files: RELEASE.md step 5."; \
	read -r -p "Publish qlcconsole v$$v from $$(git rev-parse --abbrev-ref HEAD) @ $$(git rev-parse --short HEAD)? Type 'publish' to go on: " a; \
	[ "$$a" = publish ] || { echo "Not published."; exit 1; }; \
	./release.sh
