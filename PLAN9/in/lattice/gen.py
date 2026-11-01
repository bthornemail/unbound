#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own lattice tools (runtime/lattice: compiler/peer-observer.sh
reading beacons from standard input, reconcile/tick.sh running compiler/graph-basis-compiler.sh and
compiler/plan-projector.sh), each case in a root of its own holding a copy of runtime/lattice. Run with
LC_ALL=C (the tools' sort -u, sort -nr and *.json glob follow the locale, so another locale writes
another graph), a stub ip that answers nothing (the projector takes the host's address from the
machine's routes; then it is 127.0.0.1), and a stub reconcile/rebind.sh (the real one starts the posix
bus's TCP listener). The port is literal: its trace lines carry no clock "t", and tick keeps the last
plan itself where the tool keeps its sha256. Kept, the case's root written ROOT and each trace line's
"t" taken out: observed.jsonl, discovery.log, peergraph.json, basis.json, connection-plan.json,
device-plan.json, routing.log, and each step's exit and stderr line.
	python3 gen.py <metaverse-build/runtime/lattice> <out>"""
import json, os, random, re, shutil, subprocess, sys

LAT, OUT = sys.argv[1], sys.argv[2]
SCR = os.path.join(OUT, ".scratch")
n = [0]
FILES = ["peers/observe/observed.jsonl", "trace/discovery.log", "graph/peergraph.json", "graph/basis.json",
         "plan/connection-plan.json", "plan/device-plan.json", "trace/routing.log"]

def env():
    e = dict(os.environ, LC_ALL="C", PATH=f"{SCR}/bin:" + os.environ["PATH"])
    return e

def case(seeds, steps):
    d = f"{OUT}/l{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect", exist_ok=True)
    os.makedirs(f"{d}/seeds.d", exist_ok=True)
    for name, data in seeds.items():
        open(f"{d}/seeds.d/{name}", "w").write(data)
    root = os.path.realpath(f"{d}/.run")
    shutil.rmtree(root, ignore_errors=True)
    lat = f"{root}/runtime/lattice"
    os.makedirs(f"{lat}/peers/seeds.d")
    for sub in ["compiler", "reconcile"]:
        shutil.copytree(f"{LAT}/{sub}", f"{lat}/{sub}")
    open(f"{lat}/reconcile/rebind.sh", "w").write("#!/usr/bin/env bash\necho rebind >> \"$(dirname \"$0\")/../plan/rebinds\"\n")
    for name in seeds:
        shutil.copy(f"{d}/seeds.d/{name}", f"{lat}/peers/seeds.d/{name}")
    lines = []
    for i, (kind, arg) in enumerate(steps):
        if kind == "observe":
            open(f"{d}/beacons.{i}", "w").write(arg)
            p = subprocess.run(["bash", f"{lat}/compiler/peer-observer.sh"], input=arg, env=dict(env(), BEACON_IN="-"), capture_output=True, text=True)
            lines.append(f"observe beacons.{i}")
        else:
            p = subprocess.run(["bash", f"{lat}/reconcile/tick.sh"], env=dict(env(), MAX_PEERS=str(arg)), capture_output=True, text=True)
            lines.append(f"tick {arg}")
        lines[-1] += f" {p.returncode} " + (p.stderr.splitlines() or [""])[0]
    open(f"{d}/steps", "w").write("".join(l.rstrip() + "\n" for l in lines))
    for f in FILES:
        if os.path.exists(f"{lat}/{f}"):
            s = open(f"{lat}/{f}").read().replace(root, "ROOT")
            s = re.sub(r'^\{"t":[0-9]+,', "{", s, flags=re.M)
            open(f"{d}/expect/{os.path.basename(f)}", "w").write(s)
    rb = f"{lat}/plan/rebinds"
    open(f"{d}/expect/rebinds", "w").write(str(len(open(rb).read().splitlines())) if os.path.exists(rb) else "0")
    shutil.rmtree(root)

os.makedirs(f"{SCR}/bin", exist_ok=True)
open(f"{SCR}/bin/ip", "w").write("#!/bin/sh\nexit 0\n"); os.chmod(f"{SCR}/bin/ip", 0o755)

NODES = ["host-a", "host-b", "host-c", "Host-d", "host-1", "host10", "host_e", "esp32-001", "esp32-002"]
def beacon(r, node):
    key = "id" if r.random() < 0.15 else "node"
    b = {"type": "beacon", key: node, "profile": "kernel-v1"}
    if r.random() < 0.9: b["addr"] = r.choice(["127.0.0.1", "10.0.0.%d" % r.randint(2, 9), "192.168.1.%d" % r.randint(2, 99)])
    ports = {}
    if r.random() < 0.85: ports["bus"] = r.choice([7000, 7001, 7002, 7100])
    ports["ui"] = 8080
    b["ports"] = ports
    b["caps"] = ["bus", "ui"]
    if r.random() < 0.7: b["rtt_ms"] = r.choice([1, 5, 50, 120, 1000, 99999, 100000, 150000])
    b["epoch"] = 1769892451 + r.randint(0, 99)
    s = json.dumps(b, separators=(",", ":"))
    if r.random() < 0.08: s = s[:-1] + ',"meta":{"id":"inner-%d"}}' % r.randint(1, 3)   # a later "id": sed's greedy match takes it
    return s

def seed(i, addr, bus, trailing=True):
    s = json.dumps({"id": f"host-00{i}", "addr": addr, "roles": ["kernel"], "ports": {"plan": 7070 + i, "bus": bus}}, separators=(",", ":"))
    return s + ("\n" if trailing else "")

for i in range(48):
    r = random.Random(1000 + i)
    seeds = {}
    for k in range(i % 4):
        seeds[f"host-00{k+1}.json"] = seed(k + 1, "127.0.0.1", 7000 + k, trailing=not (i % 9 == 4 and k == 0))
    steps = []
    for b in range(1 + i % 3):
        text = "".join(beacon(r, r.choice(NODES[: 3 + i % 7])) + "\n" for _ in range(r.randint(1, 6)))
        if i % 11 == 5: text = "\n" + text   # a blank line
        if i % 13 == 7: text = text[:-1]   # the last beacon without its newline
        steps.append(("observe", text))
        if b == 0 or i % 2: steps.append(("tick", [1, 1, 2, 3, 0][i % 5]))
    if i % 6 == 3: steps.append(("tick", [1, 1, 2, 3, 0][i % 5]))   # again, the plan unchanged
    case(seeds, steps)

shutil.rmtree(SCR)
print(n[0], "cases;", sum(int(open(f"{OUT}/{d}/expect/rebinds").read()) for d in os.listdir(OUT) if d.startswith("l")), "rebinds")
