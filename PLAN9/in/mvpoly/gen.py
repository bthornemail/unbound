#!/usr/bin/env python3
"""gen.py - on the host, with Metaverse Kit's own poly tools (tools/mv-poly-basis, mv-poly-closed-config,
mv-poly-decompose, mv-poly-signal-project, and their tools/wave28-poly-lib.js; node). Each case is a list
of steps run by the tools over inputs carrying digests, and the same steps for the port over the same
inputs in literal form. The port is literal (the author's choice): the tools stamp each artifact with a
sha256 of itself and name each input by its sha256; the port's artifacts carry no digest and hold their
small inputs whole (the basis, the constraints' rows, the carrier's bits, the matrix, a decomposition's
closed config) and a large one by its path (a signal's source), and validate recomputes. So expect/ holds,
for each step, the exit, the stderr line and the stdout in the port's words (the tools' lines without
their digests), and every artifact the step writes in literal form (lit.mjs, from the tool's own artifact
with the tool's own library). Cases where the port and the tool part (a check that is a digest's alone)
are not made; the tools' digest checks are recorded instead.
	python3 gen.py <metaverse-kit> <out>"""
import json, os, random, re, shutil, subprocess, sys

KIT, OUT = sys.argv[1], os.path.realpath(sys.argv[2])
HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = {"basis": "mv-poly-basis", "closed": "mv-poly-closed-config", "decompose": "mv-poly-decompose", "signal": "mv-poly-signal-project"}
n = [0]

def node(*a, cwd=None):
    return subprocess.run(["node", *a], cwd=cwd, capture_output=True, text=True)

def lit(kind, f, *inputs, cwd):
    p = node(f"{HERE}/lit.mjs", KIT, kind, f, *inputs, cwd=cwd)
    assert p.returncode == 0, p.stderr
    return p.stdout

def canon(obj):   # the tools' canonicalJson, through the library itself
    p = subprocess.run(["node", "-e", f"import('{KIT}/tools/wave28-poly-lib.js').then(L => process.stdout.write(L.canonicalJson(JSON.parse(require('fs').readFileSync(0, 'utf8')))))"], input=json.dumps(obj), capture_output=True, text=True)
    return p.stdout

def withdigest(obj):
    p = subprocess.run(["node", "-e", f"import('{KIT}/tools/wave28-poly-lib.js').then(L => {{ const o = JSON.parse(require('fs').readFileSync(0, 'utf8')); process.stdout.write(L.canonicalJson({{...o, digest: L.shaPref(Buffer.from(L.canonicalJson(o), 'utf8'))}})) }})"], input=json.dumps(obj), capture_output=True, text=True)
    return p.stdout

class Case:
    def __init__(self):
        self.d = f"{OUT}/p{n[0]:03d}"; n[0] += 1
        os.makedirs(f"{self.d}/in", exist_ok=True); os.makedirs(f"{self.d}/expect", exist_ok=True)
        self.t = os.path.realpath(f"{self.d}/.tool"); shutil.rmtree(self.t, ignore_errors=True); os.makedirs(self.t)
        self.steps = []; self.lits = {}
    def given(self, name, obj, literal=True):   # an input: with its digest for the tool, literal for the port
        open(f"{self.t}/{name}", "w").write(withdigest(obj) if literal else (obj if isinstance(obj, str) else canon(obj)))
        open(f"{self.d}/in/{name}", "w").write(canon(obj) if literal else (obj if isinstance(obj, str) else canon(obj)))
        self.lits[name] = f"{self.d}/in/{name}"
    def tamper(self, name, field, value):   # the same field changed in the tool's artifact and the port's
        k = len(self.steps)
        o = json.load(open(f"{self.t}/{name}")); o[field] = value
        open(f"{self.t}/{name}", "w").write(canon(o))
        o = json.load(open(self.lits[name])); o[field] = value
        open(f"{self.d}/expect/{k}.tamper", "w").write(canon(o))
        self.lits[name] = f"{self.d}/expect/{k}.tamper"
        self.steps.append(f"tamper {name}")
    def step(self, tool, *args, kind=None, inputs=()):
        k = len(self.steps)
        p = node(f"{KIT}/tools/{TOOLS[tool]}/index.js", *args, cwd=self.t)
        out = re.sub(r" digest=sha256:[0-9a-f]+", "", p.stdout)
        err = (p.stderr.strip().splitlines() or [""])[-1]
        self.steps.append(f"{tool} " + " ".join(args))
        open(f"{self.d}/expect/{k}.status", "w").write(f"{p.returncode}\n")
        open(f"{self.d}/expect/{k}.stderr", "w").write(err + "\n")
        open(f"{self.d}/expect/{k}.stdout", "w").write(out)
        if p.returncode == 0 and kind:
            o = args[args.index("--out") + 1]
            text = lit(kind, o, *[self.lits.get(x, x) if x.endswith(".json") else x for x in inputs], cwd=self.t)
            open(f"{self.d}/expect/{k}.{o}", "w").write(text)
            path = f"{self.d}/expect/{k}.{o}"
            self.lits[o] = path
        return p.returncode
    def done(self):
        open(f"{self.d}/steps", "w").write("".join(s + "\n" for s in self.steps))
        shutil.rmtree(self.t)

BASIS = {"authority": "advisory", "basis": ["x1", "x2", "x3", "x4", "x5", "x6"], "basis_order": ["x1", "x2", "x3", "x4", "x5", "x6"], "field": "F2", "v": "wave28.zero_poly_basis.v0", "variables": ["x1", "x2", "x3", "x4", "x5", "x6"]}
LAYOUT = "wave28.matrix_layout.core6.v0"
bits = lambda r: "".join(r.choice("01") for _ in range(6))
def poly(r):
    masks = sorted(set(r.randrange(64) for _ in range(r.randint(0, 6))), key=lambda m: format(m, "06b"))
    mon = lambda m: "1" if m == 0 else "*".join(f"x{i}" for i in range(1, 7) if m & (1 << (i - 1)))
    return "+".join(mon(m) for m in masks) or "0"

# the basis: emitted, validated, and each of its own refusals
c = Case(); c.step("basis", "emit", "--out", "basis.json", kind="basis"); c.step("basis", "validate", "--in", "basis.json"); c.done()
for field, value in [("v", "wave28.zero_poly_basis.v1"), ("authority", "binding"), ("field", "F3"), ("variables", []), ("variables", ["x1", "x1"]), ("variables", ["x7"]),
                     ("basis", ["x1+x2"]), ("basis", ["x2", "x1"]), ("basis", ["x1", "x1"]), ("basis_order", ["x1"]), ("basis", ["x1*x1"]), ("basis", ["x2*x1"]), ("basis", ["x9"]), ("basis", ["x1+"])]:
    c = Case(); c.given("basis.json", BASIS); c.tamper("basis.json", field, value); c.step("basis", "validate", "--in", "basis.json"); c.done()
c = Case(); c.step("basis", "emit"); c.step("basis", "frob"); c.step("basis", "--help"); c.done()

# the closed config over drawn constraints and carriers; then decompositions and validations
for i in range(36):
    r = random.Random(500 + i)
    c = Case()
    c.given("basis.json", BASIS)
    rows = [bits(r) for _ in range(6)]
    if i % 6 == 2: rows[5] = rows[0]   # a repeated row: the matrix may be degenerate
    c.given("constraints.json", {"authority": "advisory", "matrix_layout_id": LAYOUT, "row_masks": rows, "v": "wave28.constraints.v0"})
    c.given("carrier.json", {"authority": "advisory", "carrier_bits": bits(r), "v": "wave28.carrier_state.v0"})
    ok = c.step("closed", "build", "--basis", "basis.json", "--constraints", "constraints.json", "--carrier", "carrier.json", "--out", "closed.json", kind="closed", inputs=("basis.json", "constraints.json", "carrier.json")) == 0
    if ok:
        c.step("closed", "validate", "--in", "closed.json", "--basis", "basis.json")
        for j in range(1 + i % 3):
            pl = poly(r)
            c.step("decompose", "run", "--basis", "basis.json", "--closed", "closed.json", "--poly", pl, "--out", f"decomp{j}.json", kind="decomp", inputs=("basis.json", "closed.json"))
            c.step("decompose", "validate", "--in", f"decomp{j}.json", "--basis", "basis.json", "--closed", "closed.json")
        if i % 4 == 1:
            c.tamper("decomp0.json", "residual_poly", "x1*x2*x3*x4*x5*x6")
            c.step("decompose", "validate", "--in", "decomp0.json", "--basis", "basis.json", "--closed", "closed.json")
        if i % 4 == 3:
            c.tamper("decomp0.json", "coeff_vector", ["1", "1", "1", "1", "1", "1"])
            c.step("decompose", "validate", "--in", "decomp0.json", "--basis", "basis.json", "--closed", "closed.json")
    c.done()
# the closed config's own refusals, and the decomposition's of a polynomial not canonical
for name, field, value in [("constraints.json", "row_masks", ["000001"] * 5), ("constraints.json", "row_masks", ["00000x"] * 6), ("constraints.json", "matrix_layout_id", "other"), ("constraints.json", "v", "x"),
                           ("carrier.json", "carrier_bits", "1010"), ("carrier.json", "authority", "binding")]:
    c = Case(); c.given("basis.json", BASIS)
    c.given("constraints.json", {"authority": "advisory", "matrix_layout_id": LAYOUT, "row_masks": ["100000", "010000", "001000", "000100", "000010", "000001"], "v": "wave28.constraints.v0"})
    c.given("carrier.json", {"authority": "advisory", "carrier_bits": "101010", "v": "wave28.carrier_state.v0"})
    c.tamper(name, field, value)
    c.step("closed", "build", "--basis", "basis.json", "--constraints", "constraints.json", "--carrier", "carrier.json", "--out", "closed.json", kind="closed", inputs=("basis.json", "constraints.json", "carrier.json")); c.done()
for pl in ["x2+x1", "x1+x1", "x7", "x2*x1", "x1+", "1+x1", "x1*x2+x1", "0", "x1*x1"]:
    c = Case(); c.given("basis.json", BASIS)
    c.given("constraints.json", {"authority": "advisory", "matrix_layout_id": LAYOUT, "row_masks": ["100000", "010000", "001000", "000100", "000010", "000001"], "v": "wave28.constraints.v0"})
    c.given("carrier.json", {"authority": "advisory", "carrier_bits": "000000", "v": "wave28.carrier_state.v0"})
    c.step("closed", "build", "--basis", "basis.json", "--constraints", "constraints.json", "--carrier", "carrier.json", "--out", "closed.json", kind="closed", inputs=("basis.json", "constraints.json", "carrier.json"))
    c.step("decompose", "run", "--basis", "basis.json", "--closed", "closed.json", "--poly", pl, "--out", "decomp.json", kind="decomp", inputs=("basis.json", "closed.json")); c.done()
# signals: a pointer trace or a transmitted stream to its parity polynomial
for i in range(16):
    r = random.Random(900 + i)
    c = Case()
    if i % 2:
        text = "".join(json.dumps({"bits": bits(r), "seq": str(k)}, separators=(",", ":")) + "\n" for k in range(r.randint(1, 12)))
        if i % 7 == 5: text = text.rstrip("\n")
        c.given("txrx.ndjson", text, literal=False); src = ["--txrx", "txrx.ndjson"]
    else:
        text = "".join(json.dumps({"p_after": r.choice([r.randrange(240), r.randrange(240), 250]) if i % 8 == 6 else r.randrange(240), "seq": k}, separators=(",", ":")) + "\n" for k in range(r.randint(1, 12)))
        c.given("trace.ndjson", text, literal=False); src = ["--pointer-trace", "trace.ndjson"]
    if c.step("signal", "project", *src, "--out", "signal.json", kind="signal", inputs=(src[1], "")) == 0:
        c.step("signal", "validate", "--in", "signal.json")
    c.done()
c = Case(); c.step("signal", "project", "--out", "s.json"); c.step("signal", "project", "--txrx", "a", "--pointer-trace", "b", "--out", "s.json"); c.done()
print(n[0], "cases")
