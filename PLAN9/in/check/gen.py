#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own checkpoint tools (runtime/checkpoint: checkpoint.py,
restore.py, rolling-checkpoint.py, prune.py, window-replay.py, each running runtime/world/apply-event.py):
each case is a directory with its inputs and steps (one a line: checkpoint, restore, rolling, prune,
window, or tamper FILE WITH, which overwrites a file), run in the case's directory. The port is literal
(the author's choice): its checkpoint holds the snapshot literal where the tools hold its sha256 and a
wall-clock timestamp, restore compares bytes, prune orders by last_index. So expect/ holds what both
share: status, the stderr line, stdout (the snapshot literal where the tools print a sha256), and
checks, one a line: "bytes REL" (the file byte for byte), "ck REL" (a checkpoint's zone, trace,
snapshot, last_index, checkpoint_id, as REL.fields; and that its literal is its snapshot), "gone REL".
	python3 gen.py <metaverse-build/runtime> <world golden dir> <out>"""
import json, os, shutil, subprocess, sys

RT, WORLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
TOOL = {"checkpoint": "checkpoint/checkpoint.py", "restore": "checkpoint/restore.py", "rolling": "checkpoint/rolling-checkpoint.py",
        "prune": "checkpoint/prune.py", "window": "checkpoint/window-replay.py"}
n = [0]
FIELDS = ["zone", "trace", "snapshot", "last_index", "checkpoint_id"]

def fields(ck):
    return "|".join(json.dumps(ck.get(k), separators=(",", ":")) for k in FIELDS) + "\n"

def case(inputs, steps, checks, literal_stdout=None):
    d = f"{OUT}/k{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect/files", exist_ok=True)
    for name, data in inputs.items():
        with open(f"{d}/{name}", "w") as fh: fh.write(data)
    w = f"{d}/.run"; shutil.rmtree(w, ignore_errors=True); shutil.copytree(d, w, ignore=shutil.ignore_patterns("expect", ".run"))
    p = None
    for st in steps:
        a = st.split()
        if a[0] == "tamper":
            shutil.copy(f"{w}/{a[2]}", f"{w}/{a[1]}"); continue
        p = subprocess.run([sys.executable, f"{RT}/{TOOL[a[0]]}", *a[1:]], cwd=w, capture_output=True, text=True)
    out = p.stdout
    if literal_stdout and p.returncode == 0: out = open(f"{w}/{literal_stdout}").read()
    lines = []
    for kind, rel in checks:
        if kind == "bytes" and os.path.exists(f"{w}/{rel}"):
            os.makedirs(os.path.dirname(f"{d}/expect/files/{rel}") or ".", exist_ok=True); shutil.copy(f"{w}/{rel}", f"{d}/expect/files/{rel}"); lines.append(f"bytes {rel}")
        elif kind == "ck" and os.path.exists(f"{w}/{rel}"):
            os.makedirs(os.path.dirname(f"{d}/expect/files/{rel}") or ".", exist_ok=True)
            open(f"{d}/expect/files/{rel}.fields", "w").write(fields(json.load(open(f"{w}/{rel}")))); lines.append(f"ck {rel}")
        elif kind == "gone":
            lines.append(("gone " if not os.path.exists(f"{w}/{rel}") else "kept ") + rel)
    open(f"{d}/steps", "w").write("".join(s + "\n" for s in steps))
    open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
    open(f"{d}/expect/stderr", "w").write((p.stderr.splitlines() or [""])[0] + "\n")
    open(f"{d}/expect/stdout", "w").write(out)
    open(f"{d}/expect/checks", "w").write("".join(l + "\n" for l in lines))
    shutil.rmtree(w)

cases = [l.split("|") for l in open(f"{WORLD}/cases").read().splitlines()]
good = [c for c in cases if c[2] == "0"][:30]
bad = [c for c in cases if c[2] != "0"][:12]
snap = lambda c: open(f"{WORLD}/{c[1]}").read()
evs = lambda c: open(f"{WORLD}/{c[0]}.events").read()

for c in good[:20] + bad[:6]:   # checkpoint
    case({"base.json": snap(c), "trace.jsonl": evs(c)}, ["checkpoint zone-a base.json trace.jsonl snap.json ck.json"],
         [("bytes", "snap.json"), ("ck", "ck.json")], literal_stdout="snap.json")
for i, c in enumerate(good[:20]):   # restore: whole, a tail, a tampered snapshot, a missing field
    t = evs(c).splitlines(True); k = len(t)
    head = "".join(t[: max(1, k // 2)])
    inputs = {"base.json": snap(c), "trace.jsonl": head, "full.jsonl": evs(c), "evil.json": snap(c).replace('"room"', '"moor"', 1)}
    steps = ["checkpoint zone-a base.json trace.jsonl snap.json ck.json", "tamper trace.jsonl full.jsonl"]
    if i % 4 == 1: steps.append("tamper snap.json evil.json")
    steps.append("restore ck.json out.json")
    case(inputs, steps, [("bytes", "out.json")], literal_stdout="out.json")
for i, c in enumerate(good[:10]):   # rolling, then prune by trace position
    t = evs(c).splitlines(True)
    inputs = {"base.json": snap(c)}
    steps = []
    for j in range(1, min(len(t), 4) + 1):
        inputs[f"t{j}.jsonl"] = "".join(t[:j])
        steps.append(f"rolling zone-a base.json t{j}.jsonl cks c{j}")
    checks = [("ck", f"cks/c{j}.checkpoint.json") for j in range(1, len(steps) + 1)] + [("bytes", f"cks/c{j}.snapshot.json") for j in range(1, len(steps) + 1)]
    if i % 2:
        steps.append(f"prune cks {1 + i % 3}")
        checks = [("gone", f"cks/c{j}.checkpoint.json") for j in range(1, len(steps))] + [("gone", f"cks/c{j}.snapshot.json") for j in range(1, len(steps))]
    case(inputs, steps, checks)
for i, c in enumerate(good[:16]):   # window replay
    k = len(evs(c).splitlines())
    s, e = [(0, k - 1), (0, 0), (1, k - 1), (k - 1, k - 1), (2, 1), (0, k)][i % 6]
    case({"base.json": snap(c), "trace.jsonl": evs(c)}, [f"window base.json trace.jsonl {s} {e} out.json"], [("bytes", "out.json")], literal_stdout="out.json")
st = [open(f"{OUT}/{d}/expect/status").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("k")]
print(n[0], "cases; status:", {s: st.count(s) for s in sorted(set(st))}, "; stderr:", sorted({open(f"{OUT}/{d}/expect/stderr").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("k")}))
