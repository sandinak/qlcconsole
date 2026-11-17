#!/bin/bash
#
# Builds qlcconsole.org's content from this repo and loads it into the
# WordPress on the web server. See tools/site/README.md.
#
#   tools/site/publish.sh            # build and load
#   tools/site/publish.sh --build    # build only, into a temp dir, and say where
#
# SITE_HOST (default sandsite.org) is the ssh host; it needs sudo there and
# WP-CLI at ~/bin/wp-cli.phar (WP_CLI to change).

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
HOST="${SITE_HOST:-sandsite.org}"
# A fresh connection each time: a stale shared (ControlMaster) one hangs.
SSH=(ssh -o ControlMaster=no -o ControlPath=none -o ConnectTimeout=20 "$HOST")

VENV="${TMPDIR:-/tmp}/qlcconsole-site-venv"
if [ ! -x "$VENV/bin/python" ] || ! "$VENV/bin/python" -c "import markdown" 2>/dev/null; then
    python3 -m venv "$VENV"
    "$VENV/bin/pip" -q install markdown
fi

OUT="$(mktemp -d)"
"$VENV/bin/python" -I "$HERE/build.py" "$OUT"
if [ "${1:-}" = "--build" ]; then
    echo "built into $OUT"
    exit 0
fi

python3 -c "import json,sys; print('\n'.join(json.load(open(sys.argv[1]))['images']))" "$OUT/site.json" > "$OUT/images.txt"
"${SSH[@]}" 'rm -rf /tmp/qlcsite && mkdir -p /tmp/qlcsite/img'
RSYNC=(rsync -a --partial -e "ssh -o ControlMaster=no -o ControlPath=none -o ConnectTimeout=20")
retry() { # a flaky link drops an upload now and then; rsync picks up where it stopped
    for n in 1 2 3 4 5; do "$@" && return 0; echo "upload interrupted, retry $n" >&2; sleep 5; done
    return 1
}
(cd "$REPO" && retry "${RSYNC[@]}" --files-from="$OUT/images.txt" ./ "$HOST:/tmp/qlcsite/img/")
retry "${RSYNC[@]}" "$OUT/site.json" "$HERE/load.py" "$HOST:/tmp/qlcsite/"

# Detached, so a dropped connection can't stop it halfway; then wait for it.
"${SSH[@]}" "chmod -R a+rX /tmp/qlcsite && WP_CLI=\${WP_CLI:-\$HOME/bin/wp-cli.phar} \
    nohup sudo --preserve-env=WP_CLI python3 -u /tmp/qlcsite/load.py /tmp/qlcsite \
    > /tmp/qlcsite/load.log 2>&1 < /dev/null &"
for _ in $(seq 1 120); do
    sleep 5
    log="$("${SSH[@]}" 'cat /tmp/qlcsite/load.log')"
    if grep -qE '^DONE$|failed:|Traceback' <<<"$log"; then break; fi
done
echo "$log"
"${SSH[@]}" 'sudo rm -rf /tmp/qlcsite'
rm -rf "$OUT"
grep -q '^DONE$' <<<"$log"
