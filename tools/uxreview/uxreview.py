#!/usr/bin/env python3
#
#  Q Light Controller Plus
#  uxreview.py
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0.txt
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
"""Split screenshots into review packets, and merge the reviewers' findings.

The AI UX review (docs/dev/UX_REVIEW.md) runs one ux-reviewer subagent per
packet, all at once. This script does the parts that need no model:

  plan   <shots-dir>... --out RUN [--diff FILE] [--context SET=TEXT]... [--max N]
         Finds every PNG (and a walker's same-named .json text record),
         reads before/after and the size from the file name, groups the
         images by screen and packs screens into packets of about N images
         (default 9) with only the rules that screen can show. Writes
         RUN/pNN/packet.json, RUN/packets.json (the list, also the Workflow
         script's args) and runs each forbid_regex over the walker's text and
         the diff's added tr("...") strings (RUN/regex_hits.json). With
         --diff, one more packet (kind "diff", no images): the change read
         against the rules a diff can break before a screen shows it
         (DIFF_RULES: findable, testable where built, help units, read it
         from the device, Auto's number, the complexity bar, the flat
         surface) and the core flows whose controls it touches. Shots are
         optional with --diff.

  merge  RUN
         Reads RUN/pNN/findings.json from every reviewer, checks the fields,
         drops duplicates (the same rule, screen and quoted text at another
         size), and writes RUN/findings.json and RUN/report.md with
         thumbnails. A packet with no findings.json is listed as not
         reviewed: nothing is dropped silently.

  prompt RUN/pNN
         Prints the task line an orchestrator gives one ux-reviewer.

  selftest
         Checks the file-name reader, packing and merge on made-up data.

File names it reads: before/after anywhere as a word (before-X-1024x640,
X-after-1280x800, after-X-laptop); the size last (WxH, min, laptop, wide).
No before/after word means a current shot (a walker's), reviewed as "after".
"""
import json
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "../.."))
RULES_JSON = os.path.join(REPO, "docs/dev/ux_rules.json")

SIZE_WORDS = {"min": 0, "small": 0, "laptop": 1, "wide": 2, "large": 2}
SEVERITIES = ("blocker", "fix", "nit")
CONFIDENCES = ("high", "medium", "low")

# Rules a screenshot can show broken, for every packet.
CORE_RULES = ["UX-01", "UX-04", "UX-07", "UX-08", "UX-09", "UX-10", "UX-12",
              "UX-17", "UX-23", "UX-29", "UX-34", "UX-36", "UX-37", "UX-66"]
# More rules when the screen, set or context is about this topic.
TOPIC_RULES = [
    (r"show.?tree|cue|grey.?row|track|layer|tree", ["UX-13", "UX-15", "UX-18", "UX-22", "UX-46"]),
    (r"effect|fx|look|workbench|efx|matrix|wled", ["UX-05", "UX-20", "UX-22", "UX-47"]),
    (r"aim|formation|pan.?tilt|position|target", ["UX-47"]),
    (r"cli|command|language|typo|\bai\b|\bask|busy|didyoumean", ["UX-21", "UX-26", "UX-28", "UX-30"]),
    (r"unit|height|mount|truss|tower|platform|apron|length|properties|venue|stage",
     ["UX-02", "UX-05"]),
    (r"menu|context", ["UX-11", "UX-41", "UX-44"]),
    (r"setting|preference|option", ["UX-05", "UX-25"]),
    (r"plot|monitor|2d|3d", ["UX-39", "UX-47", "UX-55"]),
    (r"footer|chip|status|main.?bar", ["UX-33", "UX-35"]),
    (r"warn|refus|disabled|grey", ["UX-27", "UX-28", "UX-32"]),
    (r"dialog|window|picker|panel", ["UX-38"]),
    (r"wand|pixel|auto|turns|fountain|sprinkle|wled|segment", ["UX-64", "UX-65"]),
    (r"section|number|hint|list|hit|key|trigger|learn", ["UX-61", "UX-62"]),
    (r"operate|perform|\brun\b|cue.?runner", ["UX-49", "UX-50"]),
    (r"help|course|lesson|guide", ["UX-57", "UX-58", "UX-60"]),
    (r"selection|select", ["UX-43"]),
]


def load_rules(path=RULES_JSON):
    with open(path, encoding="utf-8") as fh:
        return {r["id"]: r for r in json.load(fh)}


def parse_name(stem):
    """(screen, state, size, size_rank) from a shot's file name, without .png."""
    toks = stem.split("-")
    size, rank = "", 9
    if toks and re.fullmatch(r"\d+x\d+", toks[-1]):
        size = toks.pop()
        rank = int(size.split("x")[0])
    elif toks and toks[-1].lower() in SIZE_WORDS:
        size = toks.pop()
        rank = SIZE_WORDS[size.lower()]
    state = "current"
    for s in ("before", "after"):
        if s in toks:
            state = s
            toks.remove(s)
            break
    screen = "-".join(toks) or stem
    return screen, state, size, rank


def find_shots(dirs):
    """{(set, screen): [image, ...]} from shot directories (recursively)."""
    groups = {}
    for d in dirs:
        d = os.path.abspath(d)
        for root, _dirs, names in os.walk(d):
            for n in sorted(names):
                if not n.lower().endswith(".png"):
                    continue
                path = os.path.join(root, n)
                rel = os.path.relpath(root, d)
                shot_set = os.path.basename(d) if rel == "." else \
                    os.path.basename(d) + "/" + rel
                screen, state, size, rank = parse_name(n[:-4])
                text = path[:-4] + ".json"
                groups.setdefault((shot_set, screen), []).append({
                    "file": path, "state": state, "size": size, "rank": rank,
                    "text": text if os.path.exists(text) else None})
    for imgs in groups.values():
        imgs.sort(key=lambda i: (i["rank"], i["size"], {"before": 0, "after": 1}.get(i["state"], 1)))
    return groups


def rules_for(words, rules):
    ids = list(CORE_RULES)
    for rx, extra in TOPIC_RULES:
        if re.search(rx, words, re.I):
            ids += [i for i in extra if i not in ids]
    return [i for i in sorted(ids) if i in rules]


def rule_brief(r):
    return {"id": r["id"], "title": r["title"], "rule": r["rule"],
            "right": r["example"]["right"], "wrong": r["example"]["wrong"],
            "checked_by": r.get("checks", [])}


def strings_in(obj):
    if isinstance(obj, str):
        yield obj
    elif isinstance(obj, dict):
        for v in obj.values():
            yield from strings_in(v)
    elif isinstance(obj, list):
        for v in obj:
            yield from strings_in(v)


def regex_rules(rules, scopes):
    out = []
    for r in rules.values():
        p = r.get("patterns") or {}
        if p.get("forbid_regex") and p.get("scope") in scopes:
            out.append((r["id"], re.compile(p["forbid_regex"], re.M)))
    return out


def text_hits(path, rules):
    """forbid_regex hits in a walker's text record."""
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError):
        return []
    hits = []
    for rid, rx in regex_rules(rules, ("ui_text", "docs_and_ui_text")):
        for s in strings_in(data):
            m = rx.search(s)
            if m:
                hits.append({"rule": rid, "file": path, "text": s[:200], "match": m.group(0)})
    return hits


def diff_hits(diff_path, rules):
    """forbid_regex hits in a diff's added lines: source rules on code, text
    rules on tr("...") strings, docs/course rules on added Markdown."""
    try:
        with open(diff_path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError:
        return []
    src = regex_rules(rules, ("source",))
    txt = regex_rules(rules, ("ui_text", "docs_and_ui_text"))
    docs = regex_rules(rules, ("docs_and_ui_text",))
    course = regex_rules(rules, ("course_student",))
    helps = regex_rules(rules, ("help",))
    hits, cur = [], ""
    for ln in lines:
        if ln.startswith("+++ "):
            cur = ln[6:] if ln.startswith("+++ b/") else ln[4:]
            continue
        if not ln.startswith("+") or ln.startswith("+++"):
            continue
        add = ln[1:]
        checks = []
        if cur.endswith((".cpp", ".h")) and "/test/" not in cur:
            checks += [(rid, rx, add) for rid, rx in src]
            for lit in re.findall(r'tr\(\s*"((?:[^"\\]|\\.)*)"', add):
                checks += [(rid, rx, lit) for rid, rx in txt]
        elif cur.endswith(".md"):
            checks += [(rid, rx, add) for rid, rx in docs]
            if cur.startswith("docs/features/"):
                # Help text, its code spans (typed examples) left out (UX-63).
                checks += [(rid, rx, re.sub(r"`[^`]*`", "", add)) for rid, rx in helps]
            if "course" in cur or "class" in cur:
                checks += [(rid, rx, add) for rid, rx in course]
        for rid, rx, s in checks:
            m = rx.search(s)
            if m:
                hits.append({"rule": rid, "file": cur, "text": add.strip()[:200], "match": m.group(0)})
    return hits


# What a diff is read against (the "diff" packet): the rules a change can
# break before any screen shows it, and the complexity bar.
DIFF_RULES = ["UX-01", "UX-02", "UX-12", "UX-17", "UX-23", "UX-44", "UX-60",
              "UX-61", "UX-62", "UX-63", "UX-64", "UX-65", "UX-66",
              "UX-67", "UX-68", "UX-69", "UX-70", "UX-71", "UX-72", "UX-73",
              "UX-74", "UX-75"]
DIFF_COURSE_RULES = ["UX-57", "UX-58", "UX-59"]
FLOWS_JSON = os.path.join(REPO, "docs/dev/ux_flows.json")


def load_flows(path=FLOWS_JSON):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return {}


def flow_names(flow):
    """The objectNames a flow's steps use (a diff that touches one may break it)."""
    names = set()

    def walk(st):
        if isinstance(st, list):
            for sub in st:
                walk(sub)
            return
        if not isinstance(st, dict):
            return
        d = st.get("do") or {}
        d = d if isinstance(d, dict) else {}
        for k in ("click", "choose", "menu", "action", "to"):
            if isinstance(d.get(k), str):
                names.add(d[k])
        if d.get("menu") and isinstance(d.get("item"), str):
            names.add(d["item"])
        f = st.get("find") or {}
        f = f if isinstance(f, dict) else {}
        if isinstance(f.get("widget"), str):
            names.add(f["widget"])
        c = st.get("check") or {}
        c = c if isinstance(c, dict) else {}
        for v in c.get("visible", []):
            names.add(v)
        for k in ("widgetText", "comboHas"):
            if c.get(k):
                names.add(c[k][0])
        if isinstance(c.get("window"), str) and c["window"] != "*":
            names.add(c["window"])
        if isinstance(st.get("close"), str) and st["close"] not in ("accept", "reject"):
            names.add(st["close"])
        for sub in st.get("dialog", []):
            walk(sub)
    for st in flow.get("steps", []):
        walk(st)
    return names


def touched_flows(diff_path, flows):
    """Flows whose controls (objectNames) a diff adds, removes or changes."""
    try:
        with open(diff_path, encoding="utf-8", errors="replace") as fh:
            changed = "\n".join(l for l in fh.read().splitlines()
                                if l[:1] in "+-" and not l.startswith(("+++", "---")))
    except OSError:
        return []
    out = []
    for f in flows.get("flows", []):
        hit = sorted(n for n in flow_names(f) if re.search(r"[\"']%s[\"']" % re.escape(n), changed))
        if hit:
            out.append({"flow": f["id"], "title": f["title"], "controls": hit})
    return out


def diff_packet(diff, run, pid, rules, hits):
    """One packet for the change itself: its diff read against DIFF_RULES and
    the complexity bar, with the core flows it touches."""
    flows = load_flows()
    try:
        with open(diff, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        text = ""
    ids = list(DIFF_RULES)
    if re.search(r"^\+\+\+ b/docs/course/", text, re.M):
        ids += DIFF_COURSE_RULES
    pdir = os.path.join(run, pid)
    os.makedirs(pdir, exist_ok=True)
    packet = {
        "packet": pid,
        "kind": "diff",
        "set": "diff",
        "context": "",
        "diff": os.path.abspath(diff),
        "out": pdir,
        "screens": [],
        "rules": [rule_brief(rules[r]) for r in ids if r in rules],
        "bar": flows.get("bar", {}),
        "touched_flows": touched_flows(diff, flows),
        "regex_hits": hits,
    }
    with open(os.path.join(pdir, "packet.json"), "w", encoding="utf-8") as fh:
        json.dump(packet, fh, indent=1, ensure_ascii=False)
    return {"packet": pid, "path": os.path.join(pdir, "packet.json"), "set": "diff",
            "screens": ["the diff"], "images": 0}


def plan(dirs, out, diff=None, contexts=None, max_images=9):
    rules = load_rules()
    contexts = contexts or {}
    groups = find_shots(dirs) if dirs else {}
    if not groups and not diff:
        raise SystemExit("uxreview plan: no .png under " + ", ".join(dirs))
    os.makedirs(out, exist_ok=True)
    by_set = {}
    for (s, screen), imgs in sorted(groups.items()):
        by_set.setdefault(s, []).append((screen, imgs))
    packets, n = [], 0
    for s, screens in by_set.items():
        cur = []
        for screen, imgs in screens:
            if cur and sum(len(i) for _, i in cur) + len(imgs) > max_images:
                packets.append((s, cur))
                cur = []
            cur.append((screen, imgs))
        if cur:
            packets.append((s, cur))
    hits_all = []
    if diff:
        hits_all += [dict(h, source="diff") for h in diff_hits(diff, rules)]
    index = []
    notes = []
    for s, screens in by_set.items():
        states = {i["state"] for _, imgs in screens for i in imgs}
        if "after" in states and "before" not in states:
            notes.append("%s: after shots but no before shots (UX-40)" % s)
        for screen, imgs in screens:
            st = {i["state"] for i in imgs}
            if st == {"before"}:
                notes.append("%s/%s: a before shot with no after" % (s, screen))
    for s, screens in packets:
        n += 1
        pid = "p%02d" % n
        pdir = os.path.join(os.path.abspath(out), pid)
        os.makedirs(pdir, exist_ok=True)
        words = " ".join([s] + [sc for sc, _ in screens] + [contexts.get(s, "")])
        prehits = []
        for _, imgs in screens:
            for i in imgs:
                if i["text"]:
                    prehits += [dict(h, source="walker") for h in text_hits(i["text"], rules)]
        hits_all += prehits
        packet = {
            "packet": pid,
            "set": s,
            "context": contexts.get(s, ""),
            "diff": os.path.abspath(diff) if diff else None,
            "out": pdir,
            "screens": [{"screen": sc, "images": [{k: i[k] for k in ("file", "state", "size", "text")}
                                                   for i in imgs]} for sc, imgs in screens],
            "rules": [rule_brief(rules[r]) for r in rules_for(words, rules)],
            "regex_hits": prehits,
        }
        with open(os.path.join(pdir, "packet.json"), "w", encoding="utf-8") as fh:
            json.dump(packet, fh, indent=1, ensure_ascii=False)
        index.append({"packet": pid, "path": os.path.join(pdir, "packet.json"), "set": s,
                      "screens": [sc for sc, _ in screens],
                      "images": sum(len(i) for _, i in screens)})
    run = os.path.abspath(out)
    os.makedirs(run, exist_ok=True)
    if diff:
        index.append(diff_packet(diff, run, "p%02d" % (n + 1), rules,
                                 [h for h in hits_all if h.get("source") == "diff"]))
    with open(os.path.join(run, "packets.json"), "w", encoding="utf-8") as fh:
        json.dump({"run": run, "agent": os.path.join(REPO, ".claude/agents/ux-reviewer.md"),
                   "merge": "python3 %s merge %s" % (os.path.join(HERE, "uxreview.py"), run),
                   "packets": index}, fh, indent=1, ensure_ascii=False)
    with open(os.path.join(run, "regex_hits.json"), "w", encoding="utf-8") as fh:
        json.dump(hits_all, fh, indent=1, ensure_ascii=False)
    with open(os.path.join(run, "notes.json"), "w", encoding="utf-8") as fh:
        json.dump(notes, fh, indent=1, ensure_ascii=False)
    return index, notes, hits_all


def prompt_for(packet_dir):
    p = os.path.join(os.path.abspath(packet_dir), "packet.json")
    return ("You are the ux-reviewer. Follow %s exactly. Review the packet %s and write "
            "findings.json and findings.md into %s. Reply with one line: the packet id and "
            "the number of findings (or 'no findings')."
            % (os.path.join(REPO, ".claude/agents/ux-reviewer.md"), p, os.path.dirname(p)))


def norm(s):
    s = (s or "").lower()
    s = re.sub(r"[‘’“”\"'`]", "", s)
    return re.sub(r"\s+", " ", s).strip()[:120]


def merge(run):
    run = os.path.abspath(run)
    with open(os.path.join(run, "packets.json"), encoding="utf-8") as fh:
        index = json.load(fh)["packets"]
    rules = load_rules()
    merged, problems, missing, clean = {}, [], [], []
    for p in index:
        pdir = os.path.dirname(p["path"])
        fpath = os.path.join(pdir, "findings.json")
        if not os.path.exists(fpath):
            missing.append(p["packet"])
            continue
        try:
            with open(fpath, encoding="utf-8") as fh:
                found = json.load(fh)
        except ValueError as e:
            problems.append("%s: findings.json is not JSON (%s)" % (p["packet"], e))
            missing.append(p["packet"])
            continue
        if isinstance(found, dict):
            found = found.get("findings", [])
        if not found:
            clean.append(p["packet"])
        for f in found:
            rid = f.get("rule", "")
            if rid not in rules:
                problems.append("%s: unknown rule %r (kept)" % (p["packet"], rid))
            sev = f.get("severity") if f.get("severity") in SEVERITIES else "fix"
            conf = f.get("confidence") if f.get("confidence") in CONFIDENCES else "medium"
            files = f.get("files") or ([f["file"]] if f.get("file") else [])
            # One problem, many sizes or screens: the same subject (the
            # widget, as the reviewer names it) in one set, whichever rule
            # each reviewer filed it under (UX-34 or UX-36 for one cut
            # label). Without a subject: the rule and quoted evidence on
            # one screen.
            if f.get("subject"):
                key = ("subject", p["set"], norm(f["subject"]))
            else:
                key = (rid, p["set"], f.get("screen", ""), norm(f.get("evidence")) or norm(f.get("what")))
            old = merged.get(key)
            if old:
                old["files"] += [x for x in files if x not in old["files"]]
                if f.get("screen") and f["screen"] not in old["screens"]:
                    old["screens"].append(f["screen"])
                old["regression"] = old["regression"] or bool(f.get("regression"))
                if rid not in old["rules"]:
                    old["rules"].append(rid)
                if SEVERITIES.index(sev) < SEVERITIES.index(old["severity"]):
                    old["severity"] = sev
                if CONFIDENCES.index(conf) < CONFIDENCES.index(old["confidence"]):
                    old["confidence"] = conf
                if p["packet"] not in old["packets"]:
                    old["packets"].append(p["packet"])
                continue
            merged[key] = {
                "rule": rid, "rules": [rid], "title": rules.get(rid, {}).get("title", ""),
                "set": p["set"], "screen": f.get("screen", ""),
                "screens": [f.get("screen", "")], "subject": f.get("subject", ""),
                "file": files[0] if files else "", "files": files,
                "what": f.get("what", ""), "evidence": f.get("evidence", ""),
                "severity": sev, "confidence": conf,
                "regression": bool(f.get("regression")),
                "packets": [p["packet"]],
            }
    findings = sorted(merged.values(), key=lambda f: (
        SEVERITIES.index(f["severity"]), CONFIDENCES.index(f["confidence"]), f["set"], f["screen"], f["rule"]))
    try:
        with open(os.path.join(run, "regex_hits.json"), encoding="utf-8") as fh:
            hits = json.load(fh)
    except (OSError, ValueError):
        hits = []
    try:
        with open(os.path.join(run, "notes.json"), encoding="utf-8") as fh:
            notes = json.load(fh)
    except (OSError, ValueError):
        notes = []
    result = {"run": run, "packets": len(index), "reviewed": len(index) - len(missing),
              "not_reviewed": missing, "no_findings": clean, "problems": problems,
              "notes": notes, "regex_hits": hits, "findings": findings}
    with open(os.path.join(run, "findings.json"), "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=1, ensure_ascii=False)
    with open(os.path.join(run, "report.md"), "w", encoding="utf-8") as fh:
        fh.write(report_md(result, index))
    return result


def report_md(r, index):
    run = r["run"]

    def rel(p):
        return os.path.relpath(p, run) if p and os.path.isabs(p) else p

    f = r["findings"]
    counts = {s: sum(1 for x in f if x["severity"] == s) for s in SEVERITIES}
    out = ["# UX review", ""]
    out.append("%d packets, %d reviewed. Findings: %d blocker, %d fix, %d nit%s."
               % (r["packets"], r["reviewed"], counts["blocker"], counts["fix"], counts["nit"],
                  "" if not f else " (%d high confidence)" % sum(1 for x in f if x["confidence"] == "high")))
    if r["not_reviewed"]:
        out += ["", "**Not reviewed** (no findings.json): " + ", ".join(r["not_reviewed"])
                + ". Rerun those packets before trusting this report."]
    if not f:
        out += ["", "No findings."]
    for sev in SEVERITIES:
        sel = [x for x in f if x["severity"] == sev]
        if not sel:
            continue
        out += ["", "## %s (%d)" % (sev.capitalize(), len(sel))]
        for x in sel:
            where = x["screen"] if len(x["screens"]) == 1 else \
                "%s (and %d more: %s)" % (x["screens"][0], len(x["screens"]) - 1, ", ".join(x["screens"][1:]))
            rule = x["rule"] if len(x["rules"]) == 1 else "/".join(x["rules"])
            out += ["", "### %s %s — %s/%s" % (rule, x["title"], x["set"], where),
                    "", "%s%s" % ("**Regression.** " if x["regression"] else "", x["what"]),
                    "", "> %s" % x["evidence"].replace("\n", "\n> "),
                    "", "Confidence %s · %s" % (x["confidence"], ", ".join(os.path.basename(p) for p in x["files"])),
                    ""]
            out.append(" ".join('<a href="%s"><img src="%s" width="240" alt="%s"></a>'
                                % (rel(p), rel(p), os.path.basename(p)) for p in x["files"][:3]))
    if r["regex_hits"]:
        out += ["", "## Pattern hits (forbid_regex, no model)", ""]
        for h in r["regex_hits"]:
            out.append("- %s `%s` in %s: %s" % (h["rule"], h["match"], rel(h["file"]), h["text"]))
    if r["notes"]:
        out += ["", "## Notes", ""] + ["- " + n for n in r["notes"]]
    if r["problems"]:
        out += ["", "## Reviewer output problems", ""] + ["- " + p for p in r["problems"]]
    out += ["", "## Packets", ""]
    for p in index:
        state = "not reviewed" if p["packet"] in r["not_reviewed"] else \
            "no findings" if p["packet"] in r["no_findings"] else "findings"
        md = os.path.join(os.path.dirname(p["path"]), "findings.md")
        link = "[%s](%s)" % (p["packet"], rel(md)) if os.path.exists(md) else p["packet"]
        out.append("- %s %s: %s (%d images) — %s" % (link, p["set"], ", ".join(p["screens"]), p["images"], state))
    return "\n".join(out) + "\n"


def selftest():
    ok = True

    def expect(got, want, what):
        nonlocal ok
        if got != want:
            print("uxreview selftest: %s: got %r, want %r" % (what, got, want))
            ok = False

    expect(parse_name("before-Workbench_look_aim-laptop")[:3], ("Workbench_look_aim", "before", "laptop"), "prefix state")
    expect(parse_name("mounting-height-menu-after-1024x640")[:3], ("mounting-height-menu", "after", "1024x640"), "infix state")
    expect(parse_name("greyrow-menu-after")[:3], ("greyrow-menu", "after", ""), "no size")
    expect(parse_name("showtree-typo-didyoumean-1920x1080")[:3], ("showtree-typo-didyoumean", "current", "1920x1080"), "no state")
    tmp = tempfile.mkdtemp(prefix="uxreview-selftest-")
    try:
        shots = os.path.join(tmp, "shots", "demo")
        os.makedirs(shots)
        for sc in ("alpha", "beta", "gamma"):
            for st in ("before", "after"):
                for sz in ("1024x640", "1280x800", "1920x1080"):
                    open(os.path.join(shots, "%s-%s-%s.png" % (sc, st, sz)), "wb").close()
        with open(os.path.join(shots, "alpha-after-1024x640.json"), "w") as fh:
            json.dump({"labels": ["Height above the floor (metres):"]}, fh)
        run = os.path.join(tmp, "run")
        index, _notes, hits = plan([shots], run, max_images=9)
        expect(len(index), 3, "one packet per 6-image screen at max 9")
        expect([h["rule"] for h in hits], ["UX-01"], "walker text regex hit")
        p1 = os.path.join(run, "p01")
        f = {"rule": "UX-01", "screen": "alpha", "file": os.path.join(shots, "alpha-after-1024x640.png"),
             "what": "metres", "evidence": "Height above the floor (metres):",
             "severity": "blocker", "confidence": "high"}
        with open(os.path.join(p1, "findings.json"), "w") as fh:
            json.dump([f, dict(f, file=os.path.join(shots, "alpha-after-1280x800.png"), severity="fix")], fh)
        with open(os.path.join(run, "p02", "findings.json"), "w") as fh:
            json.dump([], fh)
        r = merge(run)
        expect(len(r["findings"]), 1, "two sizes of one finding merge")
        expect(len(r["findings"][0]["files"]), 2, "merged finding keeps both files")
        expect(r["findings"][0]["severity"], "blocker", "merge keeps the worst severity")
        expect(r["not_reviewed"], ["p03"], "a packet without findings.json is listed")
        expect(r["no_findings"], ["p02"], "an empty packet is listed as clean")
        g = dict(f, rule="UX-34", subject="+ Effect button", evidence="arrow on the t, x=90")
        with open(os.path.join(run, "p02", "findings.json"), "w") as fh:
            json.dump([dict(g, screen="beta")], fh)
        with open(os.path.join(run, "p03", "findings.json"), "w") as fh:
            json.dump([dict(g, screen="gamma", rule="UX-36", evidence="arrow on the t, x=135")], fh)
        r = merge(run)
        sub = [x for x in r["findings"] if x["subject"]]
        expect(len(sub), 1, "one subject on two screens folds into one finding")
        expect(sub[0]["screens"] if sub else None, ["beta", "gamma"], "the folded finding lists both screens")
        # The diff packet: help units, a touched flow.
        dpath = os.path.join(tmp, "change.diff")
        with open(dpath, "w", encoding="utf-8") as fh:
            fh.write("+++ b/docs/features/lighting-studio.md\n"
                     "+- **Wand length**: the staff, 1.2 m unless set.\n"
                     "+- **Wand length**: the staff, 4 ft (1.2 m) unless set; typed as `1.2 m`.\n"
                     "+++ b/ui/src/showtreesections.cpp\n"
                     "+    m_prodNumbering->setObjectName(\"stProdNumbering\");\n")
        run2 = os.path.join(tmp, "run2")
        index2, _n2, hits2 = plan([], run2, diff=dpath)
        expect([h["rule"] for h in hits2 if h["rule"] == "UX-63"], ["UX-63"], "help: a metric-only length, once")
        dp = json.load(open(index2[-1]["path"], encoding="utf-8"))
        expect(dp["kind"], "diff", "the diff packet")
        expect([t["flow"] for t in dp["touched_flows"]], ["F-01"], "a diff touching stProdNumbering touches F-01")
        expect("UX-61" in [r["id"] for r in dp["rules"]], True, "the diff packet carries UX-61")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd, rest = argv[0], argv[1:]
    if cmd == "plan":
        dirs, out, diff, ctx, mx = [], None, None, {}, 9
        i = 0
        while i < len(rest):
            a = rest[i]
            if a == "--out":
                out = rest[i + 1]; i += 2
            elif a == "--diff":
                diff = rest[i + 1]; i += 2
            elif a == "--max":
                mx = int(rest[i + 1]); i += 2
            elif a == "--context":
                k, _, v = rest[i + 1].partition("="); ctx[k] = v; i += 2
            else:
                dirs.append(a); i += 1
        if (not dirs and not diff) or not out:
            print("usage: uxreview.py plan [<shots-dir>...] --out RUN [--diff FILE] [--context SET=TEXT] [--max N]")
            return 2
        index, notes, hits = plan(dirs, out, diff, ctx, mx)
        print("%d packets, %d images, %d pattern hits -> %s/packets.json"
              % (len(index), sum(p["images"] for p in index), len(hits), os.path.abspath(out)))
        for n in notes:
            print("note: " + n)
        return 0
    if cmd == "merge" and rest:
        r = merge(rest[0])
        f = r["findings"]
        print("%d findings (%s) -> %s/report.md%s"
              % (len(f), ", ".join("%d %s" % (sum(1 for x in f if x["severity"] == s), s) for s in SEVERITIES),
                 r["run"], "; NOT REVIEWED: " + ", ".join(r["not_reviewed"]) if r["not_reviewed"] else ""))
        return 1 if r["not_reviewed"] else 0
    if cmd == "prompt" and rest:
        print(prompt_for(rest[0]))
        return 0
    if cmd == "selftest":
        return 0 if selftest() else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
