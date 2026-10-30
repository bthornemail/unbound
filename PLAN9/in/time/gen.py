#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own time tools (runtime/time: branch.py, materialize.py,
merge-check.py; their checkpoints made by runtime/checkpoint/checkpoint.py): each case is a directory with
its inputs and steps (one a line: checkpoint, branch, materialize, merge, or tamper FILE WITH, which
overwrites a file), run in the case's directory. The port is literal, as zcheck: a checkpoint holds its
snapshot literal where the tools hold its sha256 and a timestamp; a branch is named by its address,
TIMELINE/PARENT, where branch.py draws a uuid; materialize prints the snapshot where the tool prints its
sha256; merge compares literals where the tool compares sha256s, so its "hash mismatch" is "literal
mismatch". expect/ holds the last step's status, its stderr line and stdout in the port's words, and
checks, one a line: "bytes REL", the file byte for byte (a branch's: the tool's bytes, its id the address).
	python3 gen.py <metaverse-build/runtime> <world golden dir> <out>"""
import json, os, re, shutil, subprocess, sys

RT, WORLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
TOOL = {"checkpoint": "checkpoint/checkpoint.py", "branch": "time/branch.py", "materialize": "time/materialize.py", "merge": "time/merge-check.py"}
n = [0]

def case(inputs, steps, checks):
    d = f"{OUT}/t{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect/files", exist_ok=True)
    for name, data in inputs.items():
        with open(f"{d}/{name}", "w") as fh: fh.write(data)
    w = f"{d}/.run"; shutil.rmtree(w, ignore_errors=True); shutil.copytree(d, w, ignore=shutil.ignore_patterns("expect", ".run"))
    p = None; ids = []
    for st in steps:
        a = st.split()
        if a[0] == "tamper":
            assert open(f"{w}/{a[1]}").read() != open(f"{w}/{a[2]}").read(), "a tamper that changes nothing"
            shutil.copy(f"{w}/{a[2]}", f"{w}/{a[1]}"); continue
        p = subprocess.run([sys.executable, f"{RT}/{TOOL[a[0]]}", *a[1:]], cwd=w, capture_output=True, text=True)
        if a[0] == "branch" and p.returncode == 0:
            assert re.fullmatch(r"branch-[0-9a-f]{8}\n", p.stdout), p.stdout
            # the port's literal id: the address it branches from; later branches read the port's own file
            tl = json.load(open(f"{w}/{a[1]}"))
            ids.append((a[3], f"{tl.get('timeline') if isinstance(tl.get('timeline'), str) else ''}/{a[2]}"))
            t = json.load(open(f"{w}/{a[3]}")); t["timeline"] = ids[-1][1]
            open(f"{w}/{a[3]}", "w").write(json.dumps(t, sort_keys=True, separators=(",", ":")))
    out, err = p.stdout, (p.stderr.splitlines() or [""])[0]
    a = steps[-1].split()
    if p.returncode == 0 and a[0] == "branch": out = ids[-1][1] + "\n"
    if p.returncode == 0 and a[0] == "materialize": out = open(f"{w}/{a[5]}").read() + "\n"
    if a[0] == "merge": err = err.replace("hash mismatch", "literal mismatch")
    lines = []
    for kind, rel in checks:
        if os.path.exists(f"{w}/{rel}"):
            shutil.copy(f"{w}/{rel}", f"{d}/expect/files/{rel}"); lines.append(f"{kind} {rel}")
    open(f"{d}/steps", "w").write("".join(s + "\n" for s in steps))
    open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
    open(f"{d}/expect/stderr", "w").write(err + "\n")
    open(f"{d}/expect/stdout", "w").write(out)
    open(f"{d}/expect/checks", "w").write("".join(l + "\n" for l in lines))
    shutil.rmtree(w)

cases = [l.split("|") for l in open(f"{WORLD}/cases").read().splitlines()]
good = [c for c in cases if c[2] == "0"]
snap = lambda c: open(f"{WORLD}/{c[1]}").read()
evs = lambda c: open(f"{WORLD}/{c[0]}.events").read()
CK = "checkpoint zone-a base.json trace.jsonl snap.json ck.json"
dumps = lambda o: json.dumps(o, separators=(",", ":")) + "\n"

# branch: timelines of one to four checkpoints, worlds named and not, a parent there and not, a branch of a branch
for i in range(40):
    k = 1 + i % 4
    nodes = {f"ck-{j}": {"checkpoint": f"ck{j}.json", "parent": f"ck-{j-1}" if j > 1 else None} for j in range(1, k + 1)}
    tl = {"world": ["room", "hall", None][i % 3], "timeline": ["main", "A", "speculative"][i % 3], "nodes": nodes}
    if i % 3 == 2: del tl["world"]
    if i % 10 == 9: del tl["nodes"]
    if i % 8 == 7: tl["note"] = "kept out of the branch"
    parent = f"ck-{1 + i % (k + 1)}"   # ck-(k+1) is not there
    steps = [f"branch timeline.json {parent} b1.json"]
    checks = [("bytes", "b1.json")]
    if i % 5 == 4 and k > 1 and parent != f"ck-{k+1}" and "nodes" in tl:
        steps.append("branch b1.json ck-1 b2.json"); checks.append(("bytes", "b2.json"))
    case({"timeline.json": dumps(tl)}, steps, checks)

# materialize: a checkpoint, then a window of its own trace (as time-tests.sh) or of the trace after it
for i, c in enumerate(good[:48]):
    t = evs(c).splitlines(True); k = len(t)
    if i % 2 == 0:
        inputs = {"base.json": snap(c), "trace.jsonl": evs(c)}; m = k
    else:
        h = max(1, k // 2); inputs = {"base.json": snap(c), "trace.jsonl": "".join(t[:h]), "next.jsonl": "".join(t[h:])}; m = k - h
    s, e = [(0, m - 1), (0, 0), (1, m - 1), (m - 1, m - 1), (2, 1), (0, m)][(i // 2) % 6]
    case(inputs, [CK, f"materialize ck.json {'trace' if i % 2 == 0 else 'next'}.jsonl {s} {e} out.json"], [("bytes", "out.json")])
for bare in [{"zone": "zone-a"}, {"zone": "zone-a", "snapshot": ""}, {"zone": "zone-a", "snapshot": None}]:
    case({"bare.json": dumps(bare), "trace.jsonl": evs(good[0])}, ["materialize bare.json trace.jsonl 0 0 out.json"], [("bytes", "out.json")])

# merge: two checkpoints, of one trace or of two, worlds the same or not, a snapshot rewritten after its checkpoint
for i, c in enumerate(good[:56]):
    t = evs(c); l = t.splitlines(True)
    tb = [t, t, "".join(l[:-1]), t + l[0]][i % 4]   # the same trace; one event short; the first again (one result, two traces)
    inputs = {"base.json": snap(c), "ta.jsonl": t, "tb.jsonl": tb,
              "tla.json": dumps({"world": "room", "timeline": "A", "nodes": {"ck-1": {"checkpoint": "cka.json", "parent": None}}}),
              "tlb.json": dumps({"world": ["room", "room", "room", "hall", "room", "room", "room"][i % 7], "timeline": "B", "nodes": {"ck-1": {"checkpoint": "ckb.json", "parent": None}}})}
    steps = ["checkpoint zone-a base.json ta.jsonl sa.json cka.json", "checkpoint zone-a base.json tb.jsonl sb.json ckb.json"]
    if i % 5 == 1: steps.append("tamper sa.json base.json")
    if i % 5 == 3: steps.append("tamper sb.json base.json")
    steps.append("merge tla.json tlb.json cka.json ckb.json")
    case(inputs, steps, [])
case({"tla.json": dumps({"world": "room"}), "tlb.json": dumps({"world": "room"}), "cka.json": dumps({"zone": "a"}), "ckb.json": dumps({"zone": "b"})},
     ["merge tla.json tlb.json cka.json ckb.json"], [])

st = [open(f"{OUT}/{d}/expect/status").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("t")]
print(n[0], "cases; status:", {s: st.count(s) for s in sorted(set(st))}, "; stderr:", sorted({open(f"{OUT}/{d}/expect/stderr").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("t")}))
