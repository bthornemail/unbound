#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own fold (runtime/world/apply-event.py): each case is a
snapshot and a list of events; the fold's exit status, its first line on standard error, and the snapshot
it writes (canonical JSON, sorted keys, no spaces), kept byte for byte. Its sha256 on standard output is
not kept: the snapshot itself is the literal. Cases: the fold's own lifecycle and authority events
(runtime/world/lifecycle-tests.sh), then lists drawn to reach every branch.
	python3 gen.py <apply-event.py> <out>"""
import json, os, random, subprocess, sys, tempfile

AE, OUT = sys.argv[1], sys.argv[2]
rnd = random.Random(0x1D1D)
SNAPS = ["room.snapshot.json", "room.authority.pre.json"]
ACTORS = ["valid:userA", "valid:userB", "", None]
ENTS = ["cube-001", "test-001", "auth-002", "e-9"]
TYPES = ["tag", "transform", "render"]
VALS = [{"label": "a"}, {"label": "b", "n": 2}, {"position": [1, 2, 3]}, {}, {"x": None, "on": True}]

CLEAN = [False]

def pick(clean, dirty):
    return rnd.choice(clean) if CLEAN[0] else rnd.choice(dirty)

def ev(t):
    e = {"type": t}
    a = pick(["valid:userA"], ACTORS)
    if a is not None: e["actor"] = a
    if t == "ENTITY_CREATE":
        e["id"] = pick(["test-001", "auth-002", "e-9"], ENTS + [7])
        o = pick(["valid:userA"], ["valid:userA", "valid:userB", ""])
        if CLEAN[0] or rnd.randrange(5): e["owner"] = o
        if rnd.randrange(4) == 0: e["internal"] = rnd.choice(["e5", "x1"])
    elif t == "ENTITY_DESTROY":
        e["id"] = pick(["test-001", "e-9"], ENTS)
    elif t in ("COMPONENT_ATTACH", "COMPONENT_UPDATE", "COMPONENT_DETACH"):
        e["entity"] = pick(["test-001", "auth-002", "e-9"], ENTS + [None])
        if e["entity"] is None: del e["entity"]
        e["component"] = rnd.choice(TYPES)
        if t == "COMPONENT_ATTACH":
            if rnd.randrange(3): e["data"] = rnd.choice(VALS)
            if rnd.randrange(4) == 0: e["cid"] = "c-" + str(rnd.randrange(9))
        if t == "COMPONENT_UPDATE" and rnd.randrange(4): e["patch"] = rnd.choice(VALS)
    elif t == "ZONE_MOVE":
        e["entity"] = pick(["test-001", "auth-002"], ENTS); e["zone"] = pick(["room-a", "room-b"], ["room-a", "room-b", 5])
    return e

def events():
    out = [ev("ENTITY_CREATE")]
    if CLEAN[0]:
        out = [{"type": "ENTITY_CREATE", "id": i, "owner": "valid:userA", "actor": "valid:userA"} for i in ("test-001", "e-9")]
    for _ in range(rnd.randrange(1, 10)):
        out.append(ev(rnd.choice(["ENTITY_CREATE", "COMPONENT_ATTACH", "COMPONENT_ATTACH", "COMPONENT_UPDATE",
                                  "COMPONENT_DETACH", "ZONE_MOVE", "ENTITY_DESTROY", "WORLD_SPIN"][: 7 if rnd.randrange(12) else 8])))
    return out

CASES = [  # the fold's own, from lifecycle-tests.sh
    ("lifecycle", "room.snapshot.json", [
        {"type": "ENTITY_CREATE", "id": "test-001", "owner": "valid:userA", "actor": "valid:userA"},
        {"type": "COMPONENT_ATTACH", "entity": "test-001", "component": "tag", "data": {"label": "test"}, "actor": "valid:userA"},
        {"type": "COMPONENT_UPDATE", "entity": "test-001", "component": "tag", "patch": {"label": "test2"}, "actor": "valid:userA"},
        {"type": "ZONE_MOVE", "entity": "test-001", "zone": "room-b", "actor": "valid:userA"}]),
    ("authpass", "room.snapshot.json", [
        {"type": "ENTITY_CREATE", "id": "auth-001", "owner": "valid:userA", "actor": "valid:userA"},
        {"type": "COMPONENT_ATTACH", "entity": "auth-001", "component": "tag", "data": {"label": "ok"}, "actor": "valid:userA"}]),
    ("authfail", "room.authority.pre.json", [
        {"type": "COMPONENT_UPDATE", "entity": "auth-002", "component": "tag", "patch": {"label": "nope"}, "actor": "valid:userB"}]),
]
for i in range(240):
    CLEAN[0] = i % 2 == 0
    CASES.append((f"r{i:03d}", SNAPS[(i // 2) % 2], events()))

lines = []
with tempfile.TemporaryDirectory() as tmp:
    for name, snap, evs in CASES:
        ef = f"{OUT}/{name}.events"
        with open(ef, "w") as fh:
            fh.write("".join(json.dumps(e, separators=(",", ":")) + "\n" for e in evs))
        of = f"{tmp}/{name}.out"
        p = subprocess.run([sys.executable, AE, f"{OUT}/{snap}", ef, of], capture_output=True, text=True)
        err = (p.stderr.splitlines() or [""])[0]
        if p.returncode == 0:
            os.replace(of, f"{OUT}/{name}.out")
        lines.append(f"{name}|{snap}|{p.returncode}|{err}")
open(f"{OUT}/cases", "w").write("\n".join(lines) + "\n")
codes = [l.split("|")[2] for l in lines]
print(len(lines), "cases:", codes.count("0"), "written,", codes.count("3"), "halted,", codes.count("2"), "refused as input;",
      sorted({l.split("|")[3] for l in lines if l.split("|")[2] != "0"}))
