#!/bin/bash
# Talking to GitHub from our Macs, the same way every time.
#
#   tools/gitsync.sh fetch             fetch origin (prune); refresh origin/*
#   tools/gitsync.sh status            origin/main here vs GitHub's main now
#   tools/gitsync.sh push-main SHA     push a gated SHA to main, guarded
#        [--dry-run] [--gate FILE]
#   tools/gitsync.sh --self-test       on scratch repos; nothing leaves the Mac
#
# Port 22 to github.com fails host-key verification on some of our networks
# (VPN, the school); then fetch and push go through ssh.github.com:443 with
# HostKeyAlias=github.com, by themselves -- no GIT_SSH_COMMAND to remember.
#
# Stale origin/main (2026-11-13): a push to a URL (`git push ssh://... SHA:main`)
# updates nothing here, so `git merge origin/main` said "Already up to date"
# and an RC was nearly built from the main before it. push-main pushes to the
# remote NAME and fetches after, and `fetch` says when origin is a local
# checkout (a clone of ~/git/qlcconsole) rather than GitHub.
#
# push-main refuses unless every guard holds (memory: guard-scripted-pushes;
# each has failed once): SHA is a full, existing commit; a gate result for it
# says gate=0 and linux=0 (the newest build-gate-logs/pool-*.txt naming
# rev=<sha12>, or --gate FILE); origin/main, freshly fetched, is an ancestor
# (a fast-forward, nothing of anyone else's dropped). After the push it
# fetches and checks origin/main is SHA. It never uses `set -e` (it does not
# hold in Claude Code's shell) and always braces "${SHA}:refs/heads/main"
# (zsh's :r modifier mangled an unbraced one).
set -u

GH443='ssh -o HostKeyAlias=github.com -o HostName=ssh.github.com -p 443'
REMOTE="${GITSYNC_REMOTE:-origin}"
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

die() { echo >&2 "gitsync: $*"; exit 1; }

url_of() { git remote get-url "$REMOTE" 2>/dev/null; }

# git with the network: as configured, then over 443 when GitHub's 22 fails.
gitnet() {
    local url; url=$(url_of)
    case "$url" in
        *ssh.github.com*)
            [ -n "${GIT_SSH_COMMAND:-}" ] || export GIT_SSH_COMMAND='ssh -o HostKeyAlias=github.com -p 443'
            git "$@"; return ;;
    esac
    git "$@" && return 0
    case "$url" in
        git@github.com:*|ssh://git@github.com/*)
            [ -n "${GIT_SSH_COMMAND:-}" ] && return 1
            echo >&2 "gitsync: github.com:22 failed; trying ssh.github.com:443"
            GIT_SSH_COMMAND="$GH443" git "$@" ;;
        *) return 1 ;;
    esac
}

do_fetch() {
    local url; url=$(url_of) || die "no remote '$REMOTE'"
    case "$url" in
        /*|file://*|../*|./*)
            echo >&2 "gitsync: NOTE: $REMOTE is a local checkout ($url), not GitHub: its main is"
            echo >&2 "  that checkout's view, which may itself be behind. For GitHub's:"
            echo >&2 "  git remote set-url $REMOTE ssh://git@ssh.github.com:443/sandinak/qlcconsole.git" ;;
    esac
    gitnet fetch --prune --quiet "$REMOTE" || die "fetch $REMOTE failed"
    echo "fetched $REMOTE: main is $(git rev-parse --short "$REMOTE/main" 2>/dev/null || echo '?')"
}

do_status() {
    local here there
    here=$(git rev-parse "$REMOTE/main" 2>/dev/null)
    there=$(gitnet ls-remote "$REMOTE" refs/heads/main | awk '{ print $1 }')
    [ -n "$there" ] || die "could not ask $REMOTE for main"
    if [ "$here" = "$there" ]; then echo "$REMOTE/main is current: ${there:0:12}"
    else echo "$REMOTE/main here is ${here:0:12}, $REMOTE has ${there:0:12}: run tools/gitsync.sh fetch"; return 1; fi
}

# The gate line for <sha>: from FILE, or the newest pool summary naming it.
gate_line() { # <sha> [file]
    local s12=${1:0:12} f
    if [ -n "${2:-}" ]; then head -n 1 "$2"; return; fi
    for f in $(ls -t build-gate-logs/pool-*.txt 2>/dev/null); do
        head -n 1 "$f" | grep -q " rev=$s12\( \|$\)" && { head -n 1 "$f"; return; }
    done
}

do_push_main() {
    local sha="" dry=0 gate="" line full
    while [ $# -gt 0 ]; do
        case "$1" in
            --dry-run) dry=1 ;;
            --gate) shift; gate="${1:-}"; [ -f "$gate" ] || die "--gate: no file '$gate'" ;;
            -*) die "push-main: unknown option $1" ;;
            *) sha="$1" ;;
        esac
        shift
    done
    [ -n "$sha" ] || die "push-main needs a SHA (refusing an empty one: it would delete main)"
    full=$(git rev-parse --verify --quiet "${sha}^{commit}") || die "'$sha' is not a commit here"
    [ "${#full}" = 40 ] || die "could not resolve '$sha' to a full SHA"
    line=$(gate_line "$full" "$gate")
    [ -n "$line" ] || die "no gate result for ${full:0:12} (build-gate-logs/pool-*.txt, or --gate FILE): gate it first"
    case "$line" in
        "gate=0 "*" rev=${full:0:12}"*" linux=0 "*|"gate=0 "*" rev=${full:0:12}"*" linux=0") ;;
        *) die "the gate for ${full:0:12} did not pass with its Linux leg: '$line'" ;;
    esac
    do_fetch >/dev/null || die "fetch failed"
    git merge-base --is-ancestor "$REMOTE/main" "$full" \
        || die "$REMOTE/main ($(git rev-parse --short "$REMOTE/main")) is not in ${full:0:12}: merge it and gate again"
    if [ "$(git rev-parse "$REMOTE/main")" = "$full" ]; then echo "$REMOTE/main is already ${full:0:12}"; return 0; fi
    echo "gate: $line"
    echo "push ${full:0:12} to $REMOTE main ($(git rev-list --count "$REMOTE/main..$full") commit(s) on $(git rev-parse --short "$REMOTE/main"))"
    [ "$dry" = 1 ] && { echo "(dry run: not pushed)"; return 0; }
    gitnet push "$REMOTE" "${full}:refs/heads/main" || die "push failed"
    do_fetch >/dev/null || die "pushed, but the fetch after failed: check with tools/gitsync.sh status"
    [ "$(git rev-parse "$REMOTE/main")" = "$full" ] || die "pushed, but $REMOTE/main is $(git rev-parse --short "$REMOTE/main"), not ${full:0:12}"
    echo "$REMOTE/main is now ${full:0:12}"
}

selftest() {
    local tmp fails=0 out a b
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/gitsync-test.XXXXXX") || return 1
    ok() { echo "ok - $1"; }
    no() { echo "not ok - $1"; fails=$((fails + 1)); }
    (
        export GIT_CONFIG_NOSYSTEM=1 HOME="$tmp" GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
        git init -q --bare -b main "$tmp/gh.git"
        git clone -q "$tmp/gh.git" "$tmp/a" 2>/dev/null
        cd "$tmp/a" && echo 1 > f && git add f && git commit -qm one && git push -q origin HEAD:main
        git clone -q "$tmp/gh.git" "$tmp/b" 2>/dev/null
        echo 2 > f && git commit -qam two
        a=$(git rev-parse HEAD)
        mkdir -p build-gate-logs
        S="$SELF"
        out=$("$S" push-main "" 2>&1) && no "an empty SHA is refused" || ok "an empty SHA is refused"
        out=$("$S" push-main "$a" 2>&1) && no "no gate result: refused" || ok "no gate result: refused"
        echo "gate=1 host=x qt=6 rev=${a:0:12} linux=0 linux_qt=6.8.2" > build-gate-logs/pool-1.txt
        out=$("$S" push-main "$a" 2>&1) && no "gate=1: refused" || ok "gate=1: refused"
        echo "gate=0 host=x qt=6 rev=${a:0:12}" > build-gate-logs/pool-2.txt
        out=$("$S" push-main "$a" 2>&1) && no "no Linux leg: refused" || ok "no Linux leg: refused"
        sleep 1; echo "gate=0 host=x qt=6 rev=${a:0:12} linux=0 linux_qt=6.8.2" > build-gate-logs/pool-3.txt
        out=$("$S" push-main --dry-run "$a" 2>&1) && printf '%s' "$out" | grep -q "dry run" \
            && [ "$(git ls-remote "$tmp/gh.git" refs/heads/main | cut -c1-40)" != "$a" ] && ok "--dry-run pushes nothing" || no "--dry-run pushes nothing: $out"
        # Someone else lands first: not a fast-forward any more.
        ( cd "$tmp/b" && echo 3 > g && git add g && git commit -qm three && git push -q origin HEAD:main )
        out=$("$S" push-main "$a" 2>&1) && no "not a fast-forward: refused" || ok "not a fast-forward: refused"
        git merge -q --no-edit origin/main >/dev/null 2>&1; b=$(git rev-parse HEAD)
        echo "gate=0 host=x qt=6 rev=${b:0:12} linux=0 linux_qt=6.8.2" > build-gate-logs/pool-4.txt
        out=$("$S" push-main "${b:0:9}" 2>&1)
        [ "$(git ls-remote "$tmp/gh.git" refs/heads/main | cut -c1-40)" = "$b" ] && ok "a gated fast-forward is pushed (short SHA given)" || no "a gated fast-forward is pushed: $out"
        [ "$(git rev-parse origin/main)" = "$b" ] && ok "origin/main is refreshed after the push" || no "origin/main is refreshed after the push"
        # A push to a URL leaves origin/main stale; fetch fixes it, status says so.
        ( cd "$tmp/b" && git pull -q --no-rebase origin main && echo 4 > h && git add h && git commit -qm four && git push -q "$tmp/gh.git" HEAD:refs/heads/main )
        "$S" status >/dev/null 2>&1 && no "status sees a stale origin/main" || ok "status sees a stale origin/main"
        "$S" fetch >/dev/null 2>&1; "$S" status >/dev/null 2>&1 && ok "fetch refreshes it" || no "fetch refreshes it"
        out=$("$S" fetch 2>&1); printf '%s' "$out" | grep -q "local checkout" && ok "fetch names a local-path origin" || no "fetch names a local-path origin"
        exit "$fails"
    ) 2>&1 | grep -E '^(not )?ok' ; fails=${PIPESTATUS[0]}
    rm -rf "$tmp"
    if [ "$fails" -eq 0 ]; then echo "gitsync.sh self-test: ok"; return 0; fi
    echo "gitsync.sh self-test: $fails failed"; return 1
}

case "${1:-}" in
    fetch) do_fetch ;;
    status) do_status ;;
    push-main) shift; do_push_main "$@" ;;
    --self-test) selftest ;;
    -h|--help|"") sed -n '2,30p' "$0" ;;
    *) die "unknown command '$1' (try --help)" ;;
esac
