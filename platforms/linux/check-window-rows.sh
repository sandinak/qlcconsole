#!/bin/bash
#
# Every top-level dialog in ui/src has a row in windowsanity_test (which opens
# it the way the app does and checks it through its window: input reaches it,
# it belongs to the main window, opens on its screen and fits, is found when
# it holds something, leaves nothing grabbed), or a line in the allowlist that
# says why not yet. A dialog added without either fails the gate here.
#
# What counts as a dialog:
#  - a class in ui/src declared ": public QDialog";
#  - a QDialog built in place in a .cpp ("QDialog dlg(...)", "new QDialog(").
#    Those are counted per file.
#
# Rows: row("Key", ...) in ui/test/windowsanity/windowsanity_test.cpp; Key is
# the class, or "<file under ui/src>#<what>" for one built in place (each
# such row counts one dialog of that file).
#
# Allowlist: platforms/linux/window-rows-allow.txt, one per line:
#     ClassName                 why it has no row yet
#     inline FILE COUNT         why COUNT dialogs built in FILE have none
# A class with a row must leave the list; a line for something gone fails.
#
#   check-window-rows.sh --source REPO     check the tree
#   check-window-rows.sh --missing REPO    print allowlist lines for what is
#                                          neither covered nor listed
#   check-window-rows.sh --self-test       this script's own test
#
# check-all.sh runs --self-test and --source before building.

TEST_REL=ui/test/windowsanity/windowsanity_test.cpp
ALLOW_REL=platforms/linux/window-rows-allow.txt

# Classes: one name per line.
dialog_classes()
{
    (cd "$1/ui/src" && grep -rhoE --include='*.h' --include='*.cpp' \
        'class +([A-Z_]+_EXPORT +)?[A-Za-z_][A-Za-z0-9_]* *(final)? *: *public +QDialog\b' . ) \
        | sed -E 's/^class +([A-Z_]+_EXPORT +)?//; s/ *(final)? *:.*$//' | sort -u
}

# Dialogs built in place: "FILE COUNT" per line.
inline_counts()
{
    # Comments (/* ... */ and //) are not code: dropped first.
    (cd "$1/ui/src" && find . -name '*.cpp' -print | sort | while read -r f; do
        n=$(perl -0pe 's{/\*.*?\*/}{}gs; s{//[^\n]*}{}g' "$f" \
            | grep -cE '(^|[^A-Za-z0-9_])QDialog +[A-Za-z_][A-Za-z0-9_]* *[({;]|new +QDialog *\(')
        [ "$n" -gt 0 ] && echo "${f#./} $n"
    done )
}

# Row keys, with repeats: one per line.
row_keys()
{
    [ -f "$1/$TEST_REL" ] || return 0
    grep -oE 'row\("[^"]+"' "$1/$TEST_REL" | sed -E 's/^row\("//; s/"$//'
}

allow_lines()
{
    [ -f "$1/$ALLOW_REL" ] || return 0
    sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$1/$ALLOW_REL"
}

check_source()
{
    local root="$1" bad=0 c f n rows allowed keys classes
    keys=$(row_keys "$root")
    classes=$(dialog_classes "$root")
    if [ -z "$classes" ]; then
        echo "check-window-rows: found no QDialog in $root/ui/src"
        return 1
    fi
    if [ ! -f "$root/$TEST_REL" ]; then
        echo "check-window-rows: $TEST_REL is missing"
        return 1
    fi
    local allow_classes allow_inline
    allow_classes=$(allow_lines "$root" | awk '$1 != "inline" { print $1 }' | sort)
    allow_inline=$(allow_lines "$root" | awk '$1 == "inline" { print $2, $3 }')

    for c in $classes; do
        local hasrow=0 listed=0
        echo "$keys" | grep -qxF "$c" && hasrow=1
        echo "$allow_classes" | grep -qxF "$c" && listed=1
        if [ $hasrow = 0 ] && [ $listed = 0 ]; then
            echo "$c: a dialog with no row in $TEST_REL. Add a row (how the app opens it), or a"
            echo "    line in $ALLOW_REL saying why not yet."
            bad=$((bad + 1))
        elif [ $hasrow = 1 ] && [ $listed = 1 ]; then
            echo "$c: has a row now; take it out of $ALLOW_REL."
            bad=$((bad + 1))
        fi
    done
    for c in $allow_classes; do
        if ! echo "$classes" | grep -qxF "$c"; then
            echo "$c: in $ALLOW_REL, but there is no such dialog in ui/src any more; take it out."
            bad=$((bad + 1))
        fi
    done

    local counts
    counts=$(inline_counts "$root")
    while read -r f n; do
        [ -n "$f" ] || continue
        rows=$(echo "$keys" | awk -v f="$f" 'index($0, f "#") == 1 { n++ } END { print n + 0 }')
        allowed=$(echo "$allow_inline" | awk -v f="$f" '$1 == f { s += $2 } END { print s + 0 }')
        if [ $((rows + allowed)) -lt "$n" ]; then
            echo "ui/src/$f: $n dialogs built in place, $rows with a row and $allowed allowed:"
            echo "    a new one needs a row (\"$f#<what>\") in $TEST_REL."
            bad=$((bad + 1))
        elif [ $((rows + allowed)) -gt "$n" ]; then
            echo "ui/src/$f: $n dialogs built in place, but $rows rows and $allowed allowed: lower its"
            echo "    count in $ALLOW_REL."
            bad=$((bad + 1))
        fi
    done <<< "$counts"
    while read -r f n; do
        [ -n "$f" ] || continue
        if ! echo "$counts" | awk '{ print $1 }' | grep -qxF "$f"; then
            echo "ui/src/$f: in $ALLOW_REL, but it builds no dialog in place any more; take it out."
            bad=$((bad + 1))
        fi
    done <<< "$allow_inline"

    if [ "$bad" -gt 0 ]; then
        echo "check-window-rows: $bad problem(s). Every window gets a row in windowsanity_test"
        echo "    (docs/dev/UX_RULES.md, R-WIN)."
        return 1
    fi
    local nc ni
    nc=$(echo "$classes" | wc -l | tr -d ' ')
    ni=$(echo "$counts" | awk '{ s += $2 } END { print s + 0 }')
    echo "check-window-rows: $nc dialog classes and $ni built in place, each with a row or allowed."
    return 0
}

print_missing()
{
    local root="$1" keys c f n rows
    keys=$(row_keys "$root")
    for c in $(dialog_classes "$root"); do
        echo "$keys" | grep -qxF "$c" && continue
        allow_lines "$root" | awk '{ print $1 }' | grep -qxF "$c" && continue
        printf '%-32s not yet walked\n' "$c"
    done
    inline_counts "$root" | while read -r f n; do
        rows=$(echo "$keys" | awk -v f="$f" 'index($0, f "#") == 1 { n++ } END { print n + 0 }')
        [ "$n" -gt "$rows" ] && printf 'inline %s %d  not yet walked\n' "$f" $((n - rows))
    done
}

self_test()
{
    local tmp fails=0 out
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/check-window-rows.XXXXXX") || return 1
    expect()
    {
        local want="$1" what="$2" rc
        shift 2
        out=$("$@" 2>&1); rc=$?
        [ "$rc" -ne 0 ] && rc=1
        if [ "$rc" -ne "$want" ]; then
            echo "self-test FAILED: $what (exit $rc, wanted $want)"
            echo "$out" | sed 's/^/    /'
            fails=$((fails + 1))
        fi
    }
    local r="$tmp/repo"
    mkdir -p "$r/ui/src/monitor" "$r/ui/test/windowsanity" "$r/platforms/linux"
    printf 'class QLC_UI_EXPORT Alpha final : public QDialog\n{\n};\n' > "$r/ui/src/alpha.h"
    printf 'class Beta : public QDialog {};\nclass NotOne : public QWidget {};\n' > "$r/ui/src/beta.h"
    printf '/* a comment\n   QDialog would; then */\n// QDialog d(x)\n' > "$r/ui/src/comment.cpp"
    printf 'void f()\n{\n    QDialog dlg(this);\n    QDialog *p = new QDialog(this);\n    QDialog *q = other;\n}\n' \
        > "$r/ui/src/monitor/plot.cpp"
    printf 'row("Alpha", x)\nrow("monitor/plot.cpp#One", x)\n' > "$r/$TEST_REL"
    printf '# why\nBeta   not yet walked\ninline monitor/plot.cpp 1  not yet\n' > "$r/$ALLOW_REL"
    expect 0 "all covered or allowed" check_source "$r"

    printf 'class Gamma : public QDialog {};\n' > "$r/ui/src/gamma.h"
    expect 1 "a new dialog class without a row" check_source "$r"
    rm "$r/ui/src/gamma.h"

    printf 'void g()\n{\n    QDialog box;\n}\n' >> "$r/ui/src/monitor/plot.cpp"
    expect 1 "a new dialog built in place without a row" check_source "$r"
    printf 'row("monitor/plot.cpp#Box", x)\n' >> "$r/$TEST_REL"
    expect 0 "the new one given a row" check_source "$r"

    printf 'void h()\n{\n    QDialog top(this);\n}\n' > "$r/ui/src/plot.cpp"
    printf 'row("monitor/plot.cpp#Extra", x)\n' >> "$r/$TEST_REL"
    printf 'inline monitor/plot.cpp 0  x\n' >> "$r/$ALLOW_REL"
    expect 1 "a row for monitor/plot.cpp does not cover plot.cpp" check_source "$r"
    rm "$r/ui/src/plot.cpp"
    sed -i.bak -e '/#Extra/d' "$r/$TEST_REL" && rm -f "$r/$TEST_REL.bak"
    sed -i.bak -e '/ 0  x$/d' "$r/$ALLOW_REL" && rm -f "$r/$ALLOW_REL.bak"
    expect 0 "back as it was" check_source "$r"

    printf 'row("Beta", x)\n' >> "$r/$TEST_REL"
    expect 1 "a class with a row still on the allowlist" check_source "$r"
    printf 'inline monitor/plot.cpp 1  not yet\n' > "$r/$ALLOW_REL"
    expect 0 "taken off" check_source "$r"

    printf 'Delta  gone\ninline monitor/plot.cpp 1  not yet\n' > "$r/$ALLOW_REL"
    expect 1 "an allowlisted class that is gone" check_source "$r"
    printf 'inline monitor/plot.cpp 2  not yet\n' > "$r/$ALLOW_REL"
    expect 1 "an inline count higher than there are" check_source "$r"
    printf 'inline monitor/plot.cpp 1  not yet\ninline gone.cpp 1 x\n' > "$r/$ALLOW_REL"
    expect 1 "an inline line for a file with none" check_source "$r"
    printf 'inline monitor/plot.cpp 1  not yet\n' > "$r/$ALLOW_REL"
    rm "$r/$TEST_REL"
    expect 1 "no windowsanity test" check_source "$r"

    rm -rf "$tmp"
    if [ "$fails" -gt 0 ]; then
        echo "check-window-rows: self-test: $fails case(s) failed."
        return 1
    fi
    echo "check-window-rows: self-test passed."
    return 0
}

case "${1:-}" in
    --source)    [ -n "${2:-}" ] || { echo >&2 "usage: $0 --source REPO"; exit 2; }
                 check_source "$2" ;;
    --missing)   [ -n "${2:-}" ] || { echo >&2 "usage: $0 --missing REPO"; exit 2; }
                 print_missing "$2" ;;
    --self-test) self_test ;;
    *)           echo >&2 "usage: $0 --source REPO | --missing REPO | --self-test"; exit 2 ;;
esac
