#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own zone tools (runtime/zones: authority-check.py,
route-event.py, interest.py, zone-materialize.py, migrate-entity.py, apply-migration.py): each case is
a directory holding its inputs, cmd (the subcommand and arguments, space-separated, for zzone), and
expect/: status (the exit code), stderr (its first line), stdout, and written (the files it wrote,
one relative path a line, each kept under expect/files/). The tool runs in the case's directory, so
the paths it writes are the paths given. migrate-entity.py and apply-migration.py print the sha256 of
what they wrote; their stdout here is that written literal instead (no hash is identity).
	python3 gen.py <metaverse-build/runtime/zones> <world golden dir> <out>"""
import json, os, random, shutil, subprocess, sys

Z, WORLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
rnd = random.Random(0x1D1D)
TOOL = {"authority": "authority-check.py", "route": "route-event.py", "interest": "interest.py",
        "materialize": "zone-materialize.py", "migrate": "migrate-entity.py", "apply": "apply-migration.py"}
n = [0]

def case(kind, args, inputs, literal_out=None):
    d = f"{OUT}/c{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect/files", exist_ok=True)
    for name, data in inputs.items():
        with open(f"{d}/{name}", "w") as fh: fh.write(data if isinstance(data, str) else json.dumps(data, separators=(",", ":")))
    before = set(os.listdir(d))
    p = subprocess.run([sys.executable, f"{Z}/{TOOL[kind]}", *args], cwd=d, capture_output=True, text=True)
    written = []
    for root, _, files in os.walk(d):
        for f in files:
            rel = os.path.relpath(os.path.join(root, f), d)
            if rel.split(os.sep)[0] in ("expect",) or rel in before: continue
            written.append(rel)
    written.sort()
    for rel in written:
        os.makedirs(os.path.dirname(f"{d}/expect/files/{rel}") or ".", exist_ok=True)
        shutil.move(f"{d}/{rel}", f"{d}/expect/files/{rel}")
    for sub in {r.split(os.sep)[0] for r in written if os.sep in r}: shutil.rmtree(f"{d}/{sub}", ignore_errors=True)
    out = p.stdout
    if literal_out and p.returncode == 0: out = open(f"{d}/expect/files/{literal_out}").read()
    open(f"{d}/cmd", "w").write(" ".join([kind, *args]) + "\n")
    open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
    open(f"{d}/expect/stderr", "w").write((p.stderr.splitlines() or [""])[0] + "\n")
    open(f"{d}/expect/stdout", "w").write(out)
    open(f"{d}/expect/written", "w").write("".join(r + "\n" for r in written))
    return d

policy = json.load(open(f"{Z}/authority-policy.json"))
pol2 = {"zone-a": ["valid:userA"], "zone-c": None}
for pz in (policy, pol2):
    for zone in ["zone-a", "zone-b", "zone-c", "room-a"]:
        for actor in ["valid:userA", "valid:userB", "valid:admin", "anon"]:
            case("authority", ["policy.json", zone, actor], {"policy.json": pz})

worlds = sorted(f for f in os.listdir(WORLD) if f.endswith(".out"))
snaps = [json.load(open(f"{WORLD}/{w}")) for w in worlds]
for i, s in enumerate(snaps[:20]):
    ents = s["state"]["entities"]
    for ev in [{"type": "ENTITY_CREATE", "id": ents[-1]["id"]}, {"type": "ZONE_MOVE", "entity": ents[0]["id"], "zone": "x"},
               {"type": "COMPONENT_UPDATE", "entity": "", "id": ents[-1]["id"]}, {"type": "ENTITY_DESTROY", "id": "nobody"},
               {"type": "ZONE_MOVE", "entity": 5}][i % 5: i % 5 + 2]:
        case("route", ["snap.json", "ev.json"], {"snap.json": s, "ev.json": ev})

for zone in ["tile-0-0", "tile--1-2", "tile-007-3", "tile-3--4", "room-a", "tile-1", "tile-a-b"]:
    for r in [None, "0", "1", "2", "-1"]:
        case("interest", [zone] + ([r] if r is not None else []), {})

for i, s in enumerate(snaps):
    case("materialize", ["snap.json", "out"], {"snap.json": s})

# migrate and apply over the zone snapshots that materialize wrote
zs = []
for d in sorted(os.listdir(OUT)):
    if not d.startswith("c"): continue
    if open(f"{OUT}/{d}/cmd").read().startswith("materialize"):
        for rel in open(f"{OUT}/{d}/expect/written").read().split():
            if rel.endswith(".snapshot.json"): zs.append(json.load(open(f"{OUT}/{d}/expect/files/{rel}")))
for i, z in enumerate(zs[:40]):
    ids = [e["id"] for e in z["state"]["entities"]]
    eid = ids[i % len(ids)] if i % 7 else "nobody"
    case("migrate", ["src.json", eid, "zone-b", "transfer.json"] + (["src-out.json"] if i % 2 else []),
         {"src.json": z if i % 9 else {k: v for k, v in z.items() if k != "zone"}}, literal_out="transfer.json")
for i, z in enumerate(zs[:40]):
    ent = z["state"]["entities"][0]
    other = zs[(i + 3) % len(zs)]
    t = {"entity": ent if i % 8 else "none", "from": z["zone"], "to": "zone-b" if i % 6 else "", "source_snapshot_hash": z.get("source_snapshot_hash")}
    case("apply", ["dest.json", "transfer.json", "dest-out.json"], {"dest.json": other if i % 5 else z, "transfer.json": t}, literal_out="dest-out.json")

st = [open(f"{OUT}/{d}/expect/status").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("c")]
print(n[0], "cases:", {k: sum(1 for d in sorted(os.listdir(OUT)) if d.startswith("c") and open(f"{OUT}/{d}/cmd").read().startswith(k)) for k in TOOL}, "status:", {s: st.count(s) for s in set(st)})
