#!/usr/bin/env python3
"""gen.py - on the host, with Metaverse Kit's own narrative world compiler (tools/mv-narrative-world-compile,
under PyYAML 6.0.1): each case a narrative YAML (the author's full narrative from speaker-for-the-untitled's
NARRATIVE_SCENE.md, the kit's sample, and narratives drawn here in the same YAML: block mappings and lists,
flow lists, quoted and plain scalars) compiled with emit and, for some, checked with validate after one of
its four files is changed or removed. The port is literal, as checkpoint, time and shards: no digests (the
world's and the manifest's digest of themselves, the world's source_digest, the manifest's world, asg and
timeline digests); validate recompiles the source and compares the four files byte for byte. So expect/
holds each file as the tool wrote it with those fields taken out and canonical again (the tool's own
canonical_json), the case's root written ROOT; emit's line with counts where the tool prints digests; and
the exit and stderr line, a validate refusal named by the file changed ("world literal mismatch" ...).
	python3 gen.py <metaverse-kit> <full narrative yaml> <out>"""
import importlib.util, json, os, random, shutil, subprocess, sys

KIT, FULL, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
TOOL = f"{KIT}/tools/mv-narrative-world-compile/index.py"
spec = importlib.util.spec_from_file_location("nwc", TOOL); nwc = importlib.util.module_from_spec(spec); spec.loader.exec_module(nwc)
FILES = ["narrative-world.v0.json", "narrative-world.asg.json", "narrative-world.timeline.ndjson", "narrative-world.manifest.json"]
NAMES = {"narrative-world.v0.json": "world", "narrative-world.asg.json": "asg", "narrative-world.timeline.ndjson": "timeline", "narrative-world.manifest.json": "manifest"}
n = [0]

def literal(name, text, root):
    text = text.replace(root, "ROOT")
    if name.endswith(".ndjson"): return text
    d = json.loads(text)
    for k in ["digest", "source_digest", "world_digest", "asg_digest", "timeline_digest"]: d.pop(k, None)
    return nwc.canonical_json(d)

def case(yaml_text, steps=("emit",), tamper=None):
    d = f"{OUT}/n{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect", exist_ok=True)
    open(f"{d}/in.yaml", "w").write(yaml_text)
    run = os.path.realpath(f"{d}/.run"); shutil.rmtree(run, ignore_errors=True); os.makedirs(run)
    shutil.copy(f"{d}/in.yaml", run)
    p = subprocess.run([sys.executable, TOOL, "emit", "--in", "in.yaml", "--out-dir", "out"], cwd=run, capture_output=True, text=True)
    emitted = p.returncode == 0
    if emitted:
        counts = json.load(open(f"{run}/out/narrative-world.manifest.json"))["counts"]
        out = f"ok mv-narrative-world-compile emit scenes={counts['scene_count']} nodes={counts['node_count']} edges={counts['edge_count']} timeline={counts['timeline_events']}\n"
        for f in FILES: open(f"{d}/expect/{f}", "w").write(literal(f, open(f"{run}/out/{f}").read(), run))
    else:
        out = p.stdout
    lines = [f"emit {p.returncode} {(p.stderr.strip().splitlines() or [''])[-1].replace(run, 'ROOT')}".rstrip()]
    open(f"{d}/expect/emit.stdout", "w").write(out)
    if emitted and tamper is not None:
        f, how = tamper
        if how == "drop": os.remove(f"{run}/out/{f}")
        else:
            t = open(f"{run}/out/{f}").read()
            open(f"{run}/out/{f}", "w").write(how(t))
        open(f"{d}/tamper", "w").write(f"{f} {'drop' if how == 'drop' else 'change'}\n")
        if how != "drop": open(f"{d}/{f}.changed", "w").write(open(f"{run}/out/{f}").read().replace(run, "ROOT"))
        q = subprocess.run([sys.executable, TOOL, "validate", "--out-dir", "out"], cwd=run, capture_output=True, text=True)
        err = (q.stderr.strip().splitlines() or [""])[-1].replace(run, "ROOT")
        if q.returncode and "missing output" not in err: err = f"ERROR: {NAMES[f]} literal mismatch"   # the port names the file changed
        lines.append(f"validate {q.returncode} {err}".rstrip())
    elif emitted and "validate" in steps:
        q = subprocess.run([sys.executable, TOOL, "validate", "--out-dir", "out"], cwd=run, capture_output=True, text=True)
        lines.append(f"validate {q.returncode} {(q.stderr.strip().splitlines() or [''])[-1]}".rstrip())
    open(f"{d}/expect/steps", "w").write("".join(l + "\n" for l in lines))
    shutil.rmtree(run)

# ── narratives drawn in the same YAML ─────────────────────────────────
def q(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t") + '"'
PLAIN = ["7", "007", "0x1F", "0o17", "017", "-3", "+4", "1_000", "2.5", "0.1", "3.0", "-0.75", "12.125", "yes", "No", "on", "OFF", "true", "False", "~", "null", "dawn", "the hour", "x-1"]
WORDS = ["gate", "breath", "plain", "Canticle of Reconciliation", "Revelation as Unveiling", "l'ordre", "élan", "logos & number", "say \"logos\"", "back\\slash", "tab\there", "名前", "αβγ"]
def scalar(r, quote_bias=0.6):
    if r.random() < quote_bias: return q(r.choice(WORDS + ["SC%03d" % r.randint(1, 20)]))
    return r.choice(PLAIN)

def narrative(r, i):
    chars = [f"char_{k}" for k in range(r.randint(1, 6))] + (["solomon"] if r.random() < .5 else [])
    sets = [f"place_{k}" for k in range(r.randint(0, 4))]
    nscenes = r.randint(1, 9)
    keys = [f"scene_{k + 1}" for k in range(nscenes)]
    if i % 7 == 3: keys = [f"scene_{k:02d}" for k in range(nscenes)]
    if i % 9 == 5: keys = [r.choice(["prologue", "scene_x", "Scene_2", "act"]) + f"_{k}" for k in range(nscenes)]
    r.shuffle(keys)
    ids = [f"SC{k:03d}" for k in range(nscenes)]
    L = ["narrative:", f"  id: {q('n%d' % i) if i % 5 else scalar(r, 0)}", f"  title: {scalar(r)}", '  version: "1.0"', '  timestamp: "2024-01-01T00:00:00Z"', "", "characters:"]
    for c in chars: L += [f"  {c}:", f"    id: {q(c.upper()[:3] + str(r.randint(1, 99))) if r.random() < .8 else r.choice(['7', '0x10', 'yes', '2.5'])}", "    role: \"x\""]
    L += ["", "settings:" + (" {}" if not sets else "")]
    for s in sets: L += [f"  {s}:", f"    id: {q('SET%03d' % r.randint(1, 9))}"] if r.random() < .85 else [f"  {s}:", "    name: \"no id\""]
    L += ["", "objects:"]
    if r.random() < .5: L[-1] += " {}"
    else:
        for o in range(r.randint(1, 3)): L += [f"  object_{o}:", f"    id: {q('OBJ%02d' % o)}"]
    L += ["", "scenes:"]
    for k, key in enumerate(keys):
        nxt = ids[(k + 1) % nscenes] if r.random() < .8 else r.choice(ids + ["SC999", "END"])
        loc = r.choice(sets + ["nowhere"]) if sets else "nowhere"
        if r.random() < .5: loc += "::" + r.choice(["gate_position", "edge", "a::b"])
        present = r.sample(chars, r.randint(0, len(chars))) + (["stranger"] if r.random() < .3 else [])
        flow = r.random() < .6
        L += [f"  {key}:", f"    id: {q(ids[k])}", f"    title: {scalar(r)}", f"    location: {q(loc)}", f"    time: {scalar(r, .4)}"]
        if flow: L += [f"    characters_present: [{', '.join(q(c) if r.random() < .7 else c for c in present)}]"]
        else: L += ["    characters_present:"] + [f"      - {q(c)}" for c in present] if present else ["    characters_present: []"]
        if r.random() < .85: L += [f"    atmosphere: {scalar(r, .5)}"]
        L += [f"    next_scene: {q(nxt)}"]
    L += ["", "transitions: {}", "relationships:", "  bonds:", "    - from: \"a\"", "      to: \"b\"", "      weight: 0.5", "thematic_clusters: {}", "narrative_metadata: {}", "trace_system: {}", ""]
    return "\n".join(L)

full = open(FULL).read()
sample = open(f"{KIT}/dev-docs/narrative-world/post-babel.sample.yaml").read()
case(full, steps=("emit", "validate"))
case(sample, steps=("emit", "validate"))
for i in range(60):
    case(narrative(random.Random(4000 + i), i), steps=("emit", "validate") if i % 4 == 0 else ("emit",))
# refusals, each the tool's own message
base = narrative(random.Random(9), 1)
case(base.replace("trace_system: {}\n", ""))   # a top-level key missing
case(base.replace("  title:", "  heading:", 1))   # narrative.title missing
case(base.replace("characters:\n", "characters: {}\nunused:\n", 1))   # characters empty
case(base.replace('    next_scene: "SC000"', '    next_scene: "SC000"', 1).replace('    id: "SC001"', '    id: "SC000"', 1))   # duplicate scene id
case(base.replace("    characters_present:", "    cast:", 1))   # a scene field missing
case(base.replace("characters:\n", "characters:\n  ghost:\n    role: \"y\"\n", 1))   # a character without id
case("- just\n- a list\n")   # the root not an object
# validate after a change or a loss, each file
for f in FILES:
    case(sample, tamper=(f, lambda t: t.replace("Canticle", "Kanticle", 1) if "Canticle" in t else t.replace("advisory", "Advisory", 1)))
    case(full, tamper=(f, "drop"))
print(n[0], "cases;", sum(1 for d in os.listdir(OUT) if d.startswith("n") and open(f"{OUT}/{d}/expect/steps").read().startswith("emit 0")), "emitted")
