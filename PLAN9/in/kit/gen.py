#!/usr/bin/env python3
"""gen.py - on the host, with Metaverse Kit's own CLIs (tools/mv-init, tools/mv-replay; node): mv-init in a
case's root, its world's files kept, run again over changed files (it never overwrites); and mv-replay over
a tile store laid out as the kit's tilestore lays it (packages/tilestore: each flushed batch of events a
segment, objects/sha256/ab/cdef..., the manifest its segments with from_event and to_event), the events the
kit's own (in/mk/events, which its validator accepts, written as JSON objects as the tilestore keeps them), cut into segments of 1 to 6 events.

The port is literal (the author's choice): a tile's events are one log, every event in order, as the Plan 9
world keeps them (mkfs: spaces/SPACE/tiles/Z/X/Y/events), no segment named by a hash. So: a full replay is
the tool's bytes; --after E is the events after E. That agrees with the tool when E ends a segment; the tool
also includes E when E begins a segment, and gives nothing when E lies inside one, the segments being cut
by its flush timer. expect/ holds the port's answer, expect/tool.stdout the tool's, and "agree" when the
two are the same. The case's root is written ROOT.
	python3 gen.py <metaverse-kit> <in/mk/events> <out>"""
import hashlib, json, os, random, shutil, subprocess, sys

KIT, EVENTS, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
n = [0]
def as_json(line):	# the kit's event, from the grammar's line form (in/mk), as a JSON object
    t = line.split(" ")
    get = lambda k: t[t.index(k) + 1] if k in t else ""
    return json.dumps({"event_id": t[1], "timestamp": get("timestamp"), "space_id": get("space_id"), "operation": get("operation"), "tile": get("tile"), "line": line}, separators=(",", ":"))
events = [as_json(l) for l in open(EVENTS).read().splitlines() if l.strip()]

def stable(v):
    if isinstance(v, dict): return {k: stable(v[k]) for k in sorted(v)}
    if isinstance(v, list): return [stable(x) for x in v]
    return v

def run(root, tool, args):
    return subprocess.run(["node", f"{KIT}/tools/{tool}/index.js", *args], cwd=root, capture_output=True, text=True)

def init_case(args, pre=None, again=False):
    d = f"{OUT}/i{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect/files", exist_ok=True)
    root = os.path.realpath(f"{d}/.run"); shutil.rmtree(root, ignore_errors=True); os.makedirs(root)
    open(f"{d}/args", "w").write(" ".join(args) + "\n")
    if pre:   # files there before mv-init runs
        for rel, text in pre.items():
            os.makedirs(os.path.dirname(f"{root}/{rel}") or root, exist_ok=True); open(f"{root}/{rel}", "w").write(text)
        os.makedirs(f"{d}/pre", exist_ok=True)
        open(f"{d}/pre/LIST", "w").write("".join(f"{rel}\n" for rel in pre))
        for i, (rel, text) in enumerate(pre.items()): open(f"{d}/pre/{i}", "w").write(text)
    p = run(root, "mv-init", args)
    if again: p = run(root, "mv-init", args)
    open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
    open(f"{d}/expect/stdout", "w").write(p.stdout.replace(root, "ROOT"))
    files = sorted(os.path.relpath(os.path.join(a, f), root) for a, _, fs in os.walk(root) for f in fs)
    open(f"{d}/expect/LIST", "w").write("".join(f"{f}\n" for f in files))
    for i, f in enumerate(files): shutil.copy(f"{root}/{f}", f"{d}/expect/files/{i}")
    shutil.rmtree(root)

def replay_case(r, k, after_kind, out_file=False):
    d = f"{OUT}/r{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect", exist_ok=True)
    root = os.path.realpath(f"{d}/.run"); shutil.rmtree(root, ignore_errors=True)
    space, tile = r.choice(["demo", "garden"]), r.choice(["z0/x0/y0", "z1/x2/y3"])
    start = r.randrange(0, len(events) - k)
    evs = events[start:start + k]
    segs, i = [], 0
    while i < len(evs):
        m = r.randint(1, 6); segs.append(evs[i:i + m]); i += m
    refs = []
    for s in segs:
        content = "\n".join(s) + "\n"
        h = hashlib.sha256(content.encode()).hexdigest()
        o = f"{root}/world/objects/sha256/{h[:2]}/{h[2:]}"
        os.makedirs(os.path.dirname(o), exist_ok=True); open(o, "w").write(content)
        refs.append({"hash": f"sha256:{h}", "from_event": json.loads(s[0])["event_id"], "to_event": json.loads(s[-1])["event_id"]})
    tdir = f"{root}/world/spaces/{space}/tiles/{tile}"
    os.makedirs(tdir, exist_ok=True)
    open(f"{tdir}/manifest.json", "w").write(json.dumps(stable({"tile_id": tile, "segments": refs}), separators=(",", ":")))
    ids = [json.loads(e)["event_id"] for e in evs]
    after = None
    if after_kind == "end": after = refs[r.randrange(len(refs))]["to_event"]
    if after_kind == "begin": after = r.choice([x["from_event"] for x in refs if x["from_event"] != x["to_event"]] or [refs[0]["from_event"]])
    if after_kind == "inside":
        inner = [s[j] for s in segs if len(s) > 2 for j in range(1, len(s) - 1)]
        after = json.loads(r.choice(inner))["event_id"] if inner else refs[0]["to_event"]
    if after_kind == "unknown": after = "no-such-event"
    args = ["--world", "world", "--space", space, "--tile", tile] + (["--after", after] if after else []) + (["--out", "out.jsonl"] if out_file else [])
    p = run(root, "mv-replay", args)
    tool = open(f"{root}/out.jsonl").read() if out_file and p.returncode == 0 else p.stdout
    # the port: the log, and after E the events after E
    log = "".join(e + "\n" for e in evs)
    if after is None: mine = log
    elif after in ids: mine = "".join(e + "\n" for e in evs[ids.index(after) + 1:])
    else: mine = ""
    os.makedirs(f"{d}/world/spaces/{space}/tiles/{tile}", exist_ok=True)
    open(f"{d}/world/spaces/{space}/tiles/{tile}/events", "w").write(log)
    open(f"{d}/args", "w").write(" ".join(args) + "\n")
    open(f"{d}/expect/tool.stdout", "w").write(tool)
    open(f"{d}/expect/stdout", "w").write(f"Wrote {mine.count(chr(10))} events to out.jsonl\n" if out_file else mine)
    if out_file: open(f"{d}/expect/out.jsonl", "w").write(mine)
    open(f"{d}/expect/agree", "w").write("yes\n" if tool == mine else "no\n")
    shutil.rmtree(root)

for args in [[], ["--space", "garden"], ["--world", "w2"], ["--world", "nested/deeper", "--space", "sky"], ["--space"], ["--space", "a", "--space", "b"], ["--world", "w", "--extra", "x"]]:
    init_case(args)
init_case([], pre={"world/.ulp-scope": "scope:\n  realm: mine\n", "world/spaces/demo/tiles/z0/x0/y0/manifest.json": "{\"tile_id\":\"z0/x0/y0\",\"segments\":[\"kept\"]}\n"})
init_case(["--space", "demo"], again=True)
for i in range(40):
    r = random.Random(700 + i)
    replay_case(r, r.randint(1, 30), [None, "end", "begin", "inside", "unknown"][i % 5], out_file=(i % 7 == 3))
print(n[0], "cases;", sum(1 for x in os.listdir(OUT) if x.startswith("r") and open(f"{OUT}/{x}/expect/agree").read() == "yes\n"), "replays agree with the tool of", sum(1 for x in os.listdir(OUT) if x.startswith("r")))
