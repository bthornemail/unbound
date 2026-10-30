#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own shard tools (runtime/shards: bundle.py, restore.py):
each case is a directory with its inputs and steps (one a line: bundle, restore, move SRC DST (the
bundle's directory copied to another node, as shard-tests.sh copies it), tamper FILE WITH (a file
overwritten), drop FILE), run in the case's directory. The port is literal, as zcheck and ztime: its
manifest keeps each file's bytes where bundle.py keeps their sha256s, and restore compares bytes, so
the tool's "hash mismatch" is "literal mismatch". expect/ holds the last step's status, stderr line and
stdout in the port's words, and checks, one a line: "bytes REL" (the file byte for byte), "fields REL"
(a manifest's zone, snapshot, trace, checkpoint, as REL.fields), "gone REL".
	python3 gen.py <metaverse-build/runtime> <world golden dir> <out>"""
import json, os, shutil, subprocess, sys

RT, WORLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
TOOL = {"bundle": "shards/bundle.py", "restore": "shards/restore.py"}
FILES = ["zone.snapshot.json", "zone.trace.jsonl", "zone.checkpoint.json"]
n = [0]

def fields(m):
    return "|".join(json.dumps(m.get(k), separators=(",", ":")) for k in ["zone", "snapshot", "trace", "checkpoint"]) + "\n"

def case(inputs, steps, checks):
    d = f"{OUT}/s{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect/files", exist_ok=True)
    for name, data in inputs.items():
        with open(f"{d}/{name}", "w") as fh: fh.write(data)
    w = f"{d}/.run"; shutil.rmtree(w, ignore_errors=True); shutil.copytree(d, w, ignore=shutil.ignore_patterns("expect", ".run"))
    p = None
    for st in steps:
        a = st.split()
        if a[0] == "move":
            shutil.copytree(f"{w}/{a[1]}", f"{w}/{a[2]}"); continue
        if a[0] == "tamper":
            assert open(f"{w}/{a[1]}").read() != open(f"{w}/{a[2]}").read(), "a tamper that changes nothing"
            shutil.copy(f"{w}/{a[2]}", f"{w}/{a[1]}"); continue
        if a[0] == "drop":
            os.remove(f"{w}/{a[1]}"); continue
        p = subprocess.run([sys.executable, f"{RT}/{TOOL[a[0]]}", *a[1:]], cwd=w, capture_output=True, text=True)
    lines = []
    for kind, rel in checks:
        if kind == "gone":
            lines.append(("gone " if not os.path.exists(f"{w}/{rel}") else "kept ") + rel)
        elif os.path.exists(f"{w}/{rel}"):
            os.makedirs(os.path.dirname(f"{d}/expect/files/{rel}"), exist_ok=True)
            if kind == "bytes": shutil.copy(f"{w}/{rel}", f"{d}/expect/files/{rel}")
            else: open(f"{d}/expect/files/{rel}.fields", "w").write(fields(json.load(open(f"{w}/{rel}"))))
            lines.append(f"{kind} {rel}")
    open(f"{d}/steps", "w").write("".join(s + "\n" for s in steps))
    open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
    open(f"{d}/expect/stderr", "w").write((p.stderr.splitlines() or [""])[0].replace("hash mismatch", "literal mismatch") + "\n")
    open(f"{d}/expect/stdout", "w").write(p.stdout)
    open(f"{d}/expect/checks", "w").write("".join(l + "\n" for l in lines))
    shutil.rmtree(w)

cases = [l.split("|") for l in open(f"{WORLD}/cases").read().splitlines()]
good = [c for c in cases if c[2] == "0"]
snap = lambda c: open(f"{WORLD}/{c[1]}").read()
out = lambda c: open(f"{WORLD}/{c[0]}.out").read()
evs = lambda c: open(f"{WORLD}/{c[0]}.events").read()
ck = lambda c, z: json.dumps({"last_index": len(evs(c).splitlines()) - 1, "snapshot": "zone.snapshot.json", "trace": "zone.trace.jsonl", "zone": z}, sort_keys=True, separators=(",", ":"))
ZONES = ["zone-a", "zone-b", "room", "z1", "plaza-01"]
DIRS = ["nodeA", "nodes/A", "shard"]

def inputs(c, z):
    return {"snap.json": out(c), "trace.jsonl": evs(c), "ck.json": ck(c, z), "base.json": snap(c), "other.jsonl": evs(c) + evs(c).splitlines(True)[0], "other.json": ck(c, z + "-x")}

def bundled(z, o):
    return [("bytes", f"{o}/{z}/{f}") for f in FILES] + [("fields", f"{o}/{z}/manifest.json")]

for i, c in enumerate(good[:30]):   # bundle
    z, o = ZONES[i % 5], DIRS[i % 3]
    case(inputs(c, z), [f"bundle {z} snap.json trace.jsonl ck.json {o}"], bundled(z, o))
for i, missing in enumerate(["snap.json", "trace.jsonl", "ck.json", "snap.json trace.jsonl", "trace.jsonl ck.json", "snap.json trace.jsonl ck.json"]):
    c = good[i]; inp = inputs(c, "zone-a")
    for m in missing.split(): del inp[m]
    case(inp, ["bundle zone-a snap.json trace.jsonl ck.json nodeA"], [("gone", "nodeA")])
for i, c in enumerate(good[:30]):   # bundle, move to another node, restore
    z = ZONES[i % 5]
    steps = [f"bundle {z} snap.json trace.jsonl ck.json nodeA"]
    if i % 3: steps.append(f"move nodeA/{z} nodeB/{z}")
    steps.append(f"restore {'nodeB' if i % 3 else 'nodeA'}/{z}")
    case(inputs(c, z), steps, bundled(z, "nodeB" if i % 3 else "nodeA"))
TAMPER = [["zone.snapshot.json base.json"], ["zone.trace.jsonl other.jsonl"], ["zone.checkpoint.json other.json"],
          ["zone.trace.jsonl other.jsonl", "zone.checkpoint.json other.json"], ["zone.snapshot.json base.json", "zone.checkpoint.json other.json"]]
for i, c in enumerate(good[:30]):   # a file rewritten in transit
    if snap(c) == out(c) and i % 5 in (0, 4): continue
    z = ZONES[i % 5]
    steps = [f"bundle {z} snap.json trace.jsonl ck.json nodeA", f"move nodeA/{z} nodeB/{z}"]
    steps += [f"tamper nodeB/{z}/{t.split()[0]} {t.split()[1]}" for t in TAMPER[i % 5]]
    steps.append(f"restore nodeB/{z}")
    case(inputs(c, z), steps, [])
for i, f in enumerate(FILES * 3):   # a file lost in transit
    c = good[i]; z = ZONES[i % 5]
    case(inputs(c, z), [f"bundle {z} snap.json trace.jsonl ck.json nodeA", f"move nodeA/{z} nodeB/{z}", f"drop nodeB/{z}/{f}", f"restore nodeB/{z}"], [])
for i in range(3):   # no manifest
    c = good[i]
    case(inputs(c, "zone-a"), ["bundle zone-a snap.json trace.jsonl ck.json nodeA", "move nodeA/zone-a nodeB/zone-a", "drop nodeB/zone-a/manifest.json", "restore nodeB/zone-a"], [])
for i, c in enumerate(good[:6]):   # bundled again over itself, then restored
    z = ZONES[i % 5]
    case(inputs(c, z), [f"bundle {z} snap.json trace.jsonl ck.json nodeA", f"bundle {z} base.json other.jsonl other.json nodeA", f"restore nodeA/{z}"], bundled(z, "nodeA"))

st = [open(f"{OUT}/{d}/expect/status").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("s")]
print(n[0], "cases; status:", {s: st.count(s) for s in sorted(set(st))}, "; stderr:", sorted({open(f"{OUT}/{d}/expect/stderr").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("s")}))
