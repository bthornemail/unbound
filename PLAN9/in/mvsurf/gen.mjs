// gen.mjs - on the host, with Metaverse Kit's own tools/mv-evidence-surface (node): each case its inputs, the
// port's arguments and what the port answers. The tool derives a surface (p0, d, k_max on the 240 ring) from
// the first three bytes of a seed digest, the evidence bundle's sha256, and names everything after it by digest;
// the port's seed is the three numbers (the author's choice, 2026-11-02: "The three numbers"), and its
// artifacts are literal: a surface holds its chords; every surface_digest is seed {d, k_max, p0}; a UART
// packet's frame_digest is the emitter frame; a wave31 receipt's packet_stream_digest and a frame verify's
// frame_stream_digest are the streams. For each seed the tool is given a seed digest whose first three bytes
// derive exactly those numbers (p0, (d - 1) / 2, k_max - 1), so its frames, LED indices and packet bytes are
// the port's goldens: an output's literal read off the tool's own (each digest checked against the literal it
// names), an input's answer the tool's own on the literal digested back, save where a case says otherwise
// (what only the three numbers can be: a d not coprime to 240, a p0 past the ring).
//	node gen.mjs <metaverse-kit> <out>
import { spawnSync } from "child_process";
import { writeFileSync, mkdirSync, readFileSync, existsSync, readdirSync, rmSync } from "fs";
import { createHash } from "crypto";
const [KIT, OUT] = process.argv.slice(2);
const TOOL = `${KIT}/tools/mv-evidence-surface/index.js`;

function ss(v) { if (Array.isArray(v)) return `[${v.map(ss).join(",")}]`; if (v && typeof v === "object") { const ks = Object.keys(v).sort(); return `{${ks.map((k) => `${JSON.stringify(k)}:${ss(v[k])}`).join(",")}}`; } return JSON.stringify(v); }
const canon = (o) => `${ss(o)}\n`;
const shaText = (s) => `sha256:${createHash("sha256").update(Buffer.from(s, "utf8")).digest("hex")}`;
const sha = (o) => shaText(canon(o));
const clone = (o) => JSON.parse(JSON.stringify(o));
const nd = (rows) => rows.map(canon).join("");
const readNd = (f) => readFileSync(f, "utf8").trimEnd().split("\n").map((l) => JSON.parse(l));
const h2 = (n) => (Number.isFinite(n) ? Math.max(0, Math.min(255, Math.floor(n))) : 0).toString(16).padStart(2, "0");
const seedDigest = (s) => `sha256:${h2(Number(s.p0))}${h2((Number(s.d) - 1) / 2)}${h2(Number(s.k_max) - 1)}${"0".repeat(58)}`;
const seedOf = (x) => ({ d: x.d, k_max: x.k_max, p0: x.p0 });

// the tool's surface for a seed, by the tool itself
const surfaces = new Map();
function toolSurface(seed) {
  const k = ss(seed);
  if (!surfaces.has(k)) {
    const d = `${OUT}/.s`; mkdirSync(d, { recursive: true });
    const p = spawnSync("node", [TOOL, "build", "--seed-digest", seedDigest(seed), "--out", "s.json"], { cwd: d, encoding: "utf8" });
    if (p.status !== 0) throw new Error(`build ${k}: ${p.stderr}`);
    const s = JSON.parse(readFileSync(`${d}/s.json`, "utf8"));
    if (ss(seedOf(s)) !== k) throw new Error(`the seed digest does not derive ${k}`);
    surfaces.set(k, s);
  }
  return surfaces.get(k);
}
// literal <-> the tool's form
const strip = (x) => { const y = { ...x }; delete y.digest; return y; };
function litSurface(s) { const L = strip(s); delete L.seed_digest; delete L.chords_digest; return L; }	// chords added by the caller
function toolOf(kind, L) {	// the literal digested back
  const x = { ...L };
  if (kind === "surface") { if ("chords" in x) { delete x.chords; x.chords_digest = sha(L.chords); } x.seed_digest = seedDigest(L); }
  if ("seed" in x) { delete x.seed; x.surface_digest = toolSurface(L.seed).digest; }
  if (kind === "packet" && "frame" in x) { delete x.frame; x.frame_digest = toolOf("emitter", L.frame).digest; }
  x.digest = sha(x);
  return x;
}
function litOf(kind, x, surface, frame) {	// the tool's artifact made literal, each digest checked against its literal
  const L = strip(x);
  if ("frame_digest" in L) { if (toolOf("emitter", frame).digest !== L.frame_digest) throw new Error("frame_digest"); delete L.frame_digest; L.frame = frame; }
  if (kind === "surface") throw new Error("surface");
  if ("surface_digest" in L) { if (L.surface_digest !== surface.digest) throw new Error("surface_digest"); delete L.surface_digest; L.seed = seedOf(surface); }
  if ("seed_digest" in L) { if (L.seed_digest !== surface.seed_digest) throw new Error("seed_digest"); delete L.seed_digest; L.seed = seedOf(surface); }
  if (canon(toolOf(kind, L)) !== canon(x) && kind !== "render") throw new Error(`${kind}: the literal does not digest back`);
  return L;
}

let n = 0;
const counts = {};
const MSG = [["surface_digest mismatch", "seed mismatch"], ["frame_digest mismatch", "frame mismatch"], ["evidence_surface chords_digest mismatch", "evidence_surface chords mismatch"],
  ["mv-evidence-surface build --seed-digest <sha256:...> --out <surface.json>", "mv-evidence-surface build --p0 <0..239> --d <1..119> --k-max <1..48> --out <surface.json>"]];
const words = (s) => { for (const [a, b] of MSG) s = s.split(a).join(b); return s.replace(/ digest=sha256:[0-9a-f]{64}/g, ""); };
// a case: files (literal for the port, the tool's form beside them as tool.*), the tool's arguments, the port's,
// and how each output the tool writes becomes the port's: { name: [kind, rows?] }
function kase(name, files, toolArgs, portArgs, outs = {}, opt = {}) {
  const d = `${OUT}/${name}${String(n++).padStart(3, "0")}`;
  mkdirSync(d, { recursive: true });
  for (const [f, v] of Object.entries(files)) writeFileSync(`${d}/${f}`, v);
  const p = spawnSync("node", [TOOL, ...toolArgs], { cwd: d, encoding: "utf8" });
  writeFileSync(`${d}/args`, portArgs.join(" ") + "\n");
  let status = p.status, stdout = words(p.stdout), stderr = words(p.stderr);
  const expect = {};
  if (p.status === 0) for (const [f, [kind, surface, frames]] of Object.entries(outs)) {
    const raw = readFileSync(`${d}/tool.${f}`);
    if (kind === "bin") expect[f] = raw;
    else if (kind === "surface") { const s = JSON.parse(raw.toString("utf8")); expect[f] = canon({ ...litSurface(s), chords: opt.chords }); }
    else if (f.endsWith(".ndjson")) expect[f] = nd(readNd(`${d}/tool.${f}`).map((x, i) => litOf(kind, x, surface, frames && frames[i])));
    else expect[f] = canon(litOf(kind, JSON.parse(raw.toString("utf8")), surface));
  }
  if (opt.literal && p.status === 0) Object.assign(expect, opt.literal(d, expect));
  if (opt.override) { writeFileSync(`${d}/tool.verdict`, `${p.status} ${p.stderr}${p.stdout}`); ({ status, stdout, stderr } = opt.override); }
  writeFileSync(`${d}/expect.status`, `${status === 0 ? 0 : 2}\n`);
  writeFileSync(`${d}/expect.stdout`, stdout);
  writeFileSync(`${d}/expect.stderr`, stderr);
  if (status === 0) for (const [f, v] of Object.entries(expect)) writeFileSync(`${d}/expect.${f}`, v);
  counts[name] = (counts[name] || 0) + 1;
  return { status, expect, dir: d };
}
const refuse = (msg) => ({ status: 2, stdout: "", stderr: `ERROR: ${msg}\n` });

// the chords, as the tool's render gives them
function chordsOf(seed) {
  const s = toolSurface(seed), d = `${OUT}/.s`;
  writeFileSync(`${d}/s.json`, canon(s));
  spawnSync("node", [TOOL, "render", "--in", "s.json", "--mode", "chords", "--out", "r.json"], { cwd: d });
  return JSON.parse(readFileSync(`${d}/r.json`, "utf8")).chords;
}
const litSurf = (seed) => ({ ...litSurface(toolSurface(seed)), chords: chordsOf(seed) });

let rs = 9;
const rnd = (k) => { rs = (rs * 1103515245 + 12345) % 2147483648; return Math.floor(rs / 65536) % k; };
const gcd = (a, b) => (b ? gcd(b, a % b) : a);
const COPRIME = Array.from({ length: 60 }, (_, i) => 2 * i + 1).filter((d) => gcd(d, 240) === 1);
const S = (p0, d, k) => ({ d: String(d), k_max: String(k), p0: String(p0) });
const KITSEED = S(155, 79, 28);
const seeds = [KITSEED, S(0, 1, 1), S(239, 119, 48), S(0, 7, 48), S(120, 113, 2)];
for (let i = 0; i < 15; i++) seeds.push(S(rnd(240), COPRIME[rnd(COPRIME.length)], 1 + rnd(48)));

// ── build, verify, render ────────────────────────────────────────
for (const s of seeds) {
  kase("b", {}, ["build", "--seed-digest", seedDigest(s), "--out", "tool.surface.json"], ["build", "--p0", s.p0, "--d", s.d, "--k-max", s.k_max, "--out", "surface.json"], { "surface.json": ["surface"] }, { chords: chordsOf(s) });
}
// what only the three numbers can be: the tool derives them and cannot be given these
for (const [s, msg] of [[S(0, 3, 4), "evidence_surface.d must be coprime to 240"], [S(0, 2, 4), "evidence_surface.d must be coprime to 240"], [S(0, 15, 4), "evidence_surface.d must be coprime to 240"], [S(240, 7, 4), "evidence_surface.p0 out of range"], [S(0, 121, 4), "evidence_surface.d out of range"], [S(0, 7, 0), "evidence_surface.k_max out of range"], [S(0, 7, 49), "evidence_surface.k_max out of range"], [{ d: "7", k_max: "4", p0: "x" }, "evidence_surface.p0 invalid integer string"]])
  kase("b", {}, [], ["build", "--p0", s.p0, "--d", s.d, "--k-max", s.k_max, "--out", "surface.json"], {}, { override: refuse(msg) });
kase("b", {}, ["build", "--out", "x.json"], ["build", "--p0", "1", "--out", "x.json"], {}, { override: refuse("build requires --p0 --d --k-max --out") });
kase("b", {}, [], [], {}, {});
kase("b", {}, ["verify"], ["verify"]);
for (const s of seeds.slice(0, 6)) {
  const L = litSurf(s), x = toolSurface(s);
  kase("v", { "surface.json": canon(L), "tool.surface.json": canon(x) }, ["verify", "--in", "tool.surface.json"], ["verify", "--in", "surface.json"]);
  for (const mode of ["chords", "spiral"]) kase("r", { "surface.json": canon(L), "tool.surface.json": canon(x) }, ["render", "--in", "tool.surface.json", "--mode", mode, "--out", "tool.render.json"], ["render", "--in", "surface.json", "--mode", mode, "--out", "render.json"], { "render.json": ["render", x] });
}
{ const L = litSurf(KITSEED), x = toolSurface(KITSEED); kase("r", { "surface.json": canon(L), "tool.surface.json": canon(x) }, ["render", "--in", "tool.surface.json", "--mode", "rings", "--out", "tool.render.json"], ["render", "--in", "surface.json", "--mode", "rings", "--out", "render.json"]); }
// a surface's literal changed
const L0 = litSurf(KITSEED);
for (const [f, over] of [[(s) => { s.authority = "binding"; }], [(s) => { s.v = "x"; }], [(s) => { s.ring_size = "241"; }], [(s) => { s.extra = "1"; }], [(s) => { delete s.chords; }],
  [(s) => { s.chords[0].hi = "1"; }], [(s) => { s.chords.reverse(); }], [(s) => { s.p0 = "156"; }], [(s) => { s.k_max = "27"; }], [(s) => { s.d = "77"; }], [(s) => { s.p0 = "300"; }], [(s) => { s.k_max = "x"; }],
  [(s) => { s.d = "75"; }, refuse("evidence_surface.d must be coprime to 240")], [(s) => { s.d = "2"; }, refuse("evidence_surface.d must be coprime to 240")]]) {
  const s = clone(L0); f(s);
  kase("v", { "surface.json": canon(s), "tool.surface.json": canon(toolOf("surface", s)) }, ["verify", "--in", "tool.surface.json"], ["verify", "--in", "surface.json"], {}, { override: over });
}

// ── frames and emitter frames ────────────────────────────────────
const traces = [null, "commit", "fail"];
const traceFile = (t) => (t === null ? null : readFileSync(`${KIT}/dev-docs/wave27/pointer-sync.trace.${t}.ndjson`, "utf8"));
const drawnTrace = () => Array.from({ length: 1 + rnd(9) }, () => canon({ p_after: String(rnd(240)), tick: String(rnd(99)) })).join("");
const framesOf = {};
let fi = 0;
for (const s of seeds.slice(0, 10)) {
  const L = litSurf(s), x = toolSurface(s);
  const tr = fi % 4 === 3 ? drawnTrace() : traceFile(traces[fi % 3]);
  const count = String([12, 1, 30, 241, 12][fi % 5]);
  const files = { "surface.json": canon(L), "tool.surface.json": canon(x) };
  const ta = [], pa = [];
  if (tr) { files["trace.ndjson"] = tr; ta.push("--pointer-trace", "trace.ndjson"); pa.push("--pointer-trace", "trace.ndjson"); }
  const r = kase("f", files, ["render-leds240", "--surface", "tool.surface.json", "--frames", count, "--out", "tool.frames.ndjson", ...ta], ["render-leds240", "--surface", "surface.json", "--frames", count, "--out", "frames.ndjson", ...pa], { "frames.ndjson": ["frame", x] });
  const ms = String([50, 1, 5000, 33][fi % 4]);
  const e = kase("e", files, ["render-leds240-esp32", "--surface", "tool.surface.json", "--frames", count, "--frame-ms", ms, "--out", "tool.emitter.ndjson", ...ta], ["render-leds240-esp32", "--surface", "surface.json", "--frames", count, "--frame-ms", ms, "--out", "emitter.ndjson", ...pa], { "emitter.ndjson": ["emitter", x] });
  framesOf[ss(s)] = { s, L, x, tr, ta, pa, ms, count: Number(count), frames: r.expect["frames.ndjson"], emitter: e.expect["emitter.ndjson"] };
  fi++;
}
{ const L = litSurf(KITSEED), x = toolSurface(KITSEED); const files = { "surface.json": canon(L), "tool.surface.json": canon(x) };
  for (const [a, b] of [[["--frames", "0"], ["--frames", "0"]], [["--frames", "10001"], ["--frames", "10001"]], [["--frame-ms", "0"], ["--frame-ms", "0"]], [["--pointer-trace", "none.ndjson"], ["--pointer-trace", "none.ndjson"]]])
    kase("e", { ...files, "none.ndjson": canon({ x: "1" }) }, ["render-leds240-esp32", "--surface", "tool.surface.json", ...a, "--out", "tool.emitter.ndjson"], ["render-leds240-esp32", "--surface", "surface.json", ...b, "--out", "emitter.ndjson"]);
  kase("f", { ...files, "bad.ndjson": "{\"p_after\":\"240\"}\n" }, ["render-leds240", "--surface", "tool.surface.json", "--pointer-trace", "bad.ndjson", "--out", "tool.frames.ndjson"], ["render-leds240", "--surface", "surface.json", "--pointer-trace", "bad.ndjson", "--out", "frames.ndjson"]);
  kase("f", { ...files, "bad.ndjson": "{\"p_after\":\"1\"}" }, ["render-leds240", "--surface", "tool.surface.json", "--pointer-trace", "bad.ndjson", "--out", "tool.frames.ndjson"], ["render-leds240", "--surface", "surface.json", "--pointer-trace", "bad.ndjson", "--out", "frames.ndjson"]);
}
// verify-leds240 and -esp32: the frames the port wrote, and changed
const vrows = (kind, rows) => rows.map((r) => toolOf(kind, r));
for (const F of Object.values(framesOf).slice(0, 6)) {
  const base = { "surface.json": canon(F.L), "tool.surface.json": canon(F.x) };
  if (F.tr) base["trace.ndjson"] = F.tr;
  const fr = F.frames.toString().trimEnd().split("\n").map((l) => JSON.parse(l)), em = F.emitter.toString().trimEnd().split("\n").map((l) => JSON.parse(l));
  kase("w", { ...base, "frames.ndjson": nd(fr), "tool.frames.ndjson": nd(vrows("frame", fr)) }, ["verify-leds240", "--surface", "tool.surface.json", "--in", "tool.frames.ndjson", ...F.ta], ["verify-leds240", "--surface", "surface.json", "--in", "frames.ndjson", ...F.pa]);
  kase("w", { ...base, "emitter.ndjson": nd(em), "tool.emitter.ndjson": nd(vrows("emitter", em)) }, ["verify-leds240-esp32", "--surface", "tool.surface.json", "--in", "tool.emitter.ndjson", "--frame-ms", F.ms, ...F.ta], ["verify-leds240-esp32", "--surface", "surface.json", "--in", "emitter.ndjson", "--frame-ms", F.ms, ...F.pa]);
}
{
  const F = framesOf[ss(KITSEED)];
  const base = { "surface.json": canon(F.L), "tool.surface.json": canon(F.x) };
  const fr = F.frames.toString().trimEnd().split("\n").map((l) => JSON.parse(l)), em = F.emitter.toString().trimEnd().split("\n").map((l) => JSON.parse(l));
  for (const f of [(r) => { r[0].mode = "leds120"; }, (r) => { r[1].chord_on = ["300", "301"]; }, (r) => { r[1].chord_on.reverse(); }, (r) => { r[1].chord_dim = [...r[1].chord_on]; }, (r) => { r[2].seed = { ...r[2].seed, p0: "1" }; }, (r) => { r[2].pointer_on = ["7"]; }, (r) => { r[3].t = "4"; }, (r) => { r.splice(1, 1); }, (r) => { r[0].authority = "x"; }, (r) => { r[0].extra = "x"; }, (r) => { r[4].chord_dim = ["1", "1"]; }])
    { const r = clone(fr); f(r); kase("w", { ...base, "frames.ndjson": nd(r), "tool.frames.ndjson": nd(vrows("frame", r)) }, ["verify-leds240", "--surface", "tool.surface.json", "--in", "tool.frames.ndjson"], ["verify-leds240", "--surface", "surface.json", "--in", "frames.ndjson"]); }
  kase("w", { ...base, "frames.ndjson": nd(fr).trimEnd(), "tool.frames.ndjson": nd(vrows("frame", fr)).trimEnd() }, ["verify-leds240", "--surface", "tool.surface.json", "--in", "tool.frames.ndjson"], ["verify-leds240", "--surface", "surface.json", "--in", "frames.ndjson"]);
  kase("w", { ...base, "frames.ndjson": nd(fr) + "{bad\n", "tool.frames.ndjson": nd(vrows("frame", fr)) + "{bad\n" }, ["verify-leds240", "--surface", "tool.surface.json", "--in", "tool.frames.ndjson"], ["verify-leds240", "--surface", "surface.json", "--in", "frames.ndjson"]);
  for (const f of [(r) => { r[0].frame_ms = "51"; }, (r) => { r[1].pointer = ["1", "2"]; }, (r) => { r[1].pointer = ["3"]; }, (r) => { r[2].on = [...r[2].on, ...r[2].dim].sort((a, b) => a - b); }, (r) => { r[2].profile = "esp8266"; }, (r) => { r[5].dim = []; }])
    { const r = clone(em); f(r); kase("w", { ...base, "emitter.ndjson": nd(r), "tool.emitter.ndjson": nd(vrows("emitter", r)) }, ["verify-leds240-esp32", "--surface", "tool.surface.json", "--in", "tool.emitter.ndjson"], ["verify-leds240-esp32", "--surface", "surface.json", "--in", "emitter.ndjson"]); }
  // the frames of another seed's surface
  const G = Object.values(framesOf)[1];
  kase("w", { ...base, "frames.ndjson": G.frames, "tool.frames.ndjson": nd(vrows("frame", G.frames.toString().trimEnd().split("\n").map((l) => JSON.parse(l)))) }, ["verify-leds240", "--surface", "tool.surface.json", "--in", "tool.frames.ndjson"], ["verify-leds240", "--surface", "surface.json", "--in", "frames.ndjson"]);
}

// ── UART: emit, verify, decode, wave31 ───────────────────────────
for (const F of Object.values(framesOf)) {
  if (F.count > 30) continue;	// 241 frames are rendered and verified above; the packets need not repeat them
  const em = F.emitter.toString().trimEnd().split("\n").map((l) => JSON.parse(l));
  const base = { "surface.json": canon(F.L), "tool.surface.json": canon(F.x), "emitter.ndjson": nd(em), "tool.emitter.ndjson": nd(vrows("emitter", em)) };
  for (const crc of ["none", "crc8-xor-v0"]) {
    const c = crc === "none" && rnd(2) ? [] : ["--uart-crc", crc];
    const u = kase("u", base, ["emit-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--out", "tool.packets.ndjson", "--out-bin", "tool.packets.bin", ...c], ["emit-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--out", "packets.ndjson", "--out-bin", "packets.bin", ...c], { "packets.ndjson": ["packet", F.x, em], "packets.bin": ["bin"] });
    if (u.status !== 0) continue;
    const pk = u.expect["packets.ndjson"].toString().trimEnd().split("\n").map((l) => JSON.parse(l)), bin = u.expect["packets.bin"];
    const vb = { ...base, "packets.ndjson": nd(pk), "tool.packets.ndjson": nd(vrows("packet", pk)), "packets.bin": bin };
    kase("u", vb, ["verify-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in", "tool.packets.ndjson", "--in-bin", "packets.bin", ...c], ["verify-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in", "packets.ndjson", "--in-bin", "packets.bin", ...c]);
    kase("d", vb, ["decode-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in-bin", "packets.bin", "--out", "tool.decoded.ndjson", ...c], ["decode-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in-bin", "packets.bin", "--out", "decoded.ndjson", ...c], { "decoded.ndjson": ["packet", F.x, em] });
    kase("q", vb, ["wave31-verify", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "tool.receipt.json", "--verify-out", "tool.verify.json", ...c], ["wave31-verify", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "receipt.json", "--verify-out", "verify.json", ...c], {},
      { literal: (d) => {	// the receipt holds the packet rows and the verify the decoded frames, checked against the tool's digests
        const rc = JSON.parse(readFileSync(`${d}/tool.receipt.json`, "utf8")), vf = JSON.parse(readFileSync(`${d}/tool.verify.json`, "utf8"));
        if (rc.packet_stream_digest !== shaText(readFileSync(`${u.dir}/tool.packets.ndjson`, "utf8"))) throw new Error("packet_stream_digest");
        if (vf.frame_stream_digest !== shaText(nd(vrows("emitter", em)))) throw new Error("frame_stream_digest");
        if (rc.surface_digest !== F.x.digest || vf.surface_digest !== F.x.digest) throw new Error("surface_digest");
        const R = strip(rc), V = strip(vf);
        for (const X of [R, V]) { delete X.surface_digest; X.seed = seedOf(F.x); }
        delete R.packet_stream_digest; R.packet_stream = pk;
        delete V.frame_stream_digest; V.frame_stream = em;
        return { "receipt.json": canon(R), "verify.json": canon(V) };
      } });
  }
}
// the kit's must-reject binaries, against its golden surface and emitter frames
{
  const F = framesOf[ss(KITSEED)];
  const em = F.emitter.toString().trimEnd().split("\n").map((l) => JSON.parse(l));
  const base = { "surface.json": canon(F.L), "tool.surface.json": canon(F.x), "emitter.ndjson": nd(em), "tool.emitter.ndjson": nd(vrows("emitter", em)) };
  const MR = `${KIT}/dev-docs/wave30/must-reject`;
  for (const [f, crc] of [["bad-uart-decode-crc.bin", "crc8-xor-v0"], ["bad-uart-decode-extra.bin", "none"], ["bad-uart-decode-truncated.bin", "none"], ["bad-uart-decode-version.bin", "none"], ["bad-uart-bin-content.bin", "none"], ["bad-uart-bin-len.bin", "none"]]) {
    const b = readFileSync(`${MR}/${f}`);
    kase("d", { ...base, "packets.bin": b }, ["decode-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in-bin", "packets.bin", "--out", "tool.decoded.ndjson", "--uart-crc", crc], ["decode-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in-bin", "packets.bin", "--out", "decoded.ndjson", "--uart-crc", crc], { "decoded.ndjson": ["packet", F.x, em] });
    kase("q", { ...base, "packets.bin": b }, ["wave31-verify", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "tool.receipt.json", "--verify-out", "tool.verify.json", "--uart-crc", crc], ["wave31-verify", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "receipt.json", "--verify-out", "verify.json", "--uart-crc", crc]);
  }
  // bytes changed one at a time: the time, frame_ms, the pointer, a mask bit, the crc
  const u = readdirSync(OUT).filter((x) => x[0] === "u" && existsSync(`${OUT}/${x}/expect.packets.bin`) && readFileSync(`${OUT}/${x}/surface.json`, "utf8") === canon(F.L)).sort();	// the golden seed's two emits, without and with the crc
  const good = readFileSync(`${OUT}/${u[0]}/expect.packets.bin`);
  const crcGood = readFileSync(`${OUT}/${u[1]}/expect.packets.bin`);
  if (good.length !== 12 * 67 || crcGood.length !== 12 * 68) throw new Error("the golden emits");
  for (const [i, v, bin, crc] of [[3, 9, good, "none"], [5, 1, good, "none"], [6, 0xfe, good, "none"], [6, 241, good, "none"], [10, 0x80, good, "none"], [67 + 40, 1, good, "none"], [67, 0x31, good, "none"], [67, 0x30, crcGood, "crc8-xor-v0"], [68 + 5, 2, crcGood, "crc8-xor-v0"]]) {
    const b = Buffer.from(bin); b[i] ^= v;
    kase("q", { ...base, "packets.bin": b }, ["wave31-verify", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "tool.receipt.json", "--verify-out", "tool.verify.json", "--uart-crc", crc], ["wave31-verify", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in-bin", "packets.bin", "--receipt-out", "receipt.json", "--verify-out", "verify.json", "--uart-crc", crc]);
  }
  // the packet rows changed
  const pk = readNd(`${OUT}/${u[0]}/expect.packets.ndjson`);
  for (const f of [(r) => { r[0].packet_hex = r[0].packet_hex.replace(/^30/, "31"); }, (r) => { r[1].uart_crc = "crc8-xor-v0"; }, (r) => { r[1].packet_bytes = "68"; }, (r) => { r[2].frame = r[3].frame; }, (r) => { r[2].seed = { ...r[2].seed, d: "1" }; }, (r) => { r[0].packet_hex = r[0].packet_hex.toUpperCase(); }])
    { const r = clone(pk); f(r); kase("u", { ...base, "packets.ndjson": nd(r), "tool.packets.ndjson": nd(vrows("packet", r)) }, ["verify-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--in", "tool.packets.ndjson"], ["verify-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--in", "packets.ndjson"]); }
  kase("u", { ...base, "packets.ndjson": nd(pk), "tool.packets.ndjson": nd(vrows("packet", pk)) }, ["emit-esp32-uart", "--surface", "tool.surface.json", "--emitter", "tool.emitter.ndjson", "--out", "tool.p.ndjson", "--uart-crc", "crc16"], ["emit-esp32-uart", "--surface", "surface.json", "--emitter", "emitter.ndjson", "--out", "p.ndjson", "--uart-crc", "crc16"]);
}
rmSync(`${OUT}/.s`, { recursive: true, force: true });
console.log(n, "cases:", JSON.stringify(counts));
