// gen.mjs - on the host, with Metaverse Kit's own tools (mv-runtime-handoff, and mv-evidence-surface and
// mv-evidence-bundle to make its inputs; node): each case its inputs, the port's arguments and what the port
// answers. mv-runtime-handoff turns the wave30 surface's outputs (frames, and optionally the bundle, emitter
// frames, UART packets and the wave31 receipt and frame verify) into a world.ir.v0, or the wave31 pair alone into
// one, and verifies a world IR's shape. The tools name all of these by digest; the port's are the literals of
// in/mvsurf and in/mvevid (a surface's seed {d, k_max, p0}, a packet holding its frame, a receipt and a verify
// holding their streams, a bundle holding its subject and evidence), and its world IR holds literals where the
// tool's holds digests: an LED's surface_digest is the seed; a frame event's frame_digest is the frame; an
// attachment's ref is the surface, the frame stream, the bundle, the packet stream, the receipt or the verify,
// its kind without "_digest"; a wave31 component holds the seed and the stream, the receipt's or the verify's
// own digest gone, and an event's digest is the receipt or the verify. An output's literal is read off the
// tool's own (each digest checked against the literal it names); an input's answer is the tool's own on the
// literals digested back; verify-world-ir, which reads no digest, the tool's own on the same file.
//	node gen.mjs <metaverse-kit> <out>
import { spawnSync } from "child_process";
import { writeFileSync, mkdirSync, readFileSync, rmSync } from "fs";
import { createHash } from "crypto";
const [KIT, OUT] = process.argv.slice(2);
const T = { handoff: `${KIT}/tools/mv-runtime-handoff/index.js`, surface: `${KIT}/tools/mv-evidence-surface/index.js`, bundle: `${KIT}/tools/mv-evidence-bundle/index.js` };

function ss(v) { if (Array.isArray(v)) return `[${v.map(ss).join(",")}]`; if (v && typeof v === "object") { const ks = Object.keys(v).sort(); return `{${ks.map((k) => `${JSON.stringify(k)}:${ss(v[k])}`).join(",")}}`; } return JSON.stringify(v); }
const canon = (o) => `${ss(o)}\n`;
const shaText = (s) => `sha256:${createHash("sha256").update(Buffer.from(s, "utf8")).digest("hex")}`;
const sha = (o) => shaText(canon(o));
const clone = (o) => JSON.parse(JSON.stringify(o));
const nd = (rows) => rows.map(canon).join("");
const readNd = (f) => readFileSync(f, "utf8").trimEnd().split("\n").map((l) => JSON.parse(l));
const read = (f) => JSON.parse(readFileSync(f, "utf8"));
const strip = (x) => { const y = { ...x }; delete y.digest; return y; };
const h2 = (n) => (Number.isFinite(n) ? Math.max(0, Math.min(255, Math.floor(n))) : 0).toString(16).padStart(2, "0");
const seedDigest = (s) => `sha256:${h2(Number(s.p0))}${h2((Number(s.d) - 1) / 2)}${h2(Number(s.k_max) - 1)}${"0".repeat(58)}`;
const seedOf = (x) => ({ d: x.d, k_max: x.k_max, p0: x.p0 });
const W = `${OUT}/.w`;
const run = (tool, args, cwd = W) => spawnSync("node", [T[tool], ...args], { cwd, encoding: "utf8" });

// ── the inputs: the tool's form from the kit's own tools, and their literals ──
// every digest any input has, and the literal it names
const lit = new Map();
function chain(seed, frames, crc) {
  mkdirSync(W, { recursive: true });
  const ok = (p) => { if (p.status !== 0) throw new Error(p.stderr); };
  ok(run("surface", ["build", "--seed-digest", seedDigest(seed), "--out", "s.json"]));
  ok(run("surface", ["render", "--in", "s.json", "--mode", "chords", "--out", "r.json"]));
  ok(run("surface", ["render-leds240", "--surface", "s.json", "--frames", String(frames), "--out", "f.ndjson"]));
  ok(run("surface", ["render-leds240-esp32", "--surface", "s.json", "--frames", String(frames), "--out", "e.ndjson"]));
  ok(run("surface", ["emit-esp32-uart", "--surface", "s.json", "--emitter", "e.ndjson", "--out", "u.ndjson", "--out-bin", "u.bin", "--uart-crc", crc]));
  ok(run("surface", ["wave31-verify", "--surface", "s.json", "--emitter", "e.ndjson", "--in-bin", "u.bin", "--receipt-out", "rc.json", "--verify-out", "vf.json", "--uart-crc", crc]));
  const s = read(`${W}/s.json`), t = { surface: s, frames: readNd(`${W}/f.ndjson`), emitter: readNd(`${W}/e.ndjson`), uart: readNd(`${W}/u.ndjson`), receipt: read(`${W}/rc.json`), verify: read(`${W}/vf.json`) };
  const sd = seedOf(s), L = {};
  L.surface = { ...strip(s), chords: read(`${W}/r.json`).chords }; delete L.surface.seed_digest; delete L.surface.chords_digest;
  const swap = (x) => { const y = strip(x); if ("surface_digest" in y) { delete y.surface_digest; y.seed = sd; } return y; };
  L.frames = t.frames.map(swap);
  L.emitter = t.emitter.map(swap);
  L.uart = t.uart.map((x, i) => { const y = swap(x); if (x.frame_digest !== t.emitter[i].digest) throw new Error("frame_digest"); delete y.frame_digest; y.frame = L.emitter[i]; return y; });
  L.receipt = swap(t.receipt); delete L.receipt.packet_stream_digest; L.receipt.packet_stream = L.uart;
  L.verify = swap(t.verify); delete L.verify.frame_stream_digest; L.verify.frame_stream = L.emitter;
  if (t.receipt.packet_stream_digest !== shaText(nd(t.uart)) || t.verify.frame_stream_digest !== shaText(nd(t.emitter))) throw new Error("streams");
  lit.set(s.digest, L.surface);
  t.frames.forEach((x, i) => lit.set(x.digest, L.frames[i]));
  lit.set(shaText(nd(t.frames)), L.frames);
  lit.set(shaText(nd(t.uart)), L.uart);
  lit.set(t.receipt.digest, L.receipt);
  lit.set(t.verify.digest, L.verify);
  return { seed, t, L };
}
// the literal digested back, as the tools would have written it
const surfaces = new Map();
function surfaceDigest(seed) {
  const k = ss(seed);
  if (!surfaces.has(k)) { mkdirSync(W, { recursive: true }); run("surface", ["build", "--seed-digest", seedDigest(seed), "--out", "x.json"]); surfaces.set(k, read(`${W}/x.json`).digest); }
  return surfaces.get(k);
}
function toolOf(kind, L) {
  if (!L || typeof L !== "object" || Array.isArray(L)) return L;
  const x = { ...L };
  if (kind === "surface") { if ("chords" in x) { delete x.chords; x.chords_digest = sha(L.chords); } x.seed_digest = seedDigest(L); }
  if ("seed" in x) { delete x.seed; x.surface_digest = surfaceDigest(L.seed); }
  if (kind === "uart" && "frame" in x) { delete x.frame; x.frame_digest = toolOf("emitter", L.frame).digest; }
  if (kind === "receipt" && "packet_stream" in x) { delete x.packet_stream; x.packet_stream_digest = shaText(nd(L.packet_stream.map((r) => toolOf("uart", r)))); }
  if (kind === "verify" && "frame_stream" in x) { delete x.frame_stream; x.frame_stream_digest = shaText(nd(L.frame_stream.map((r) => toolOf("emitter", r)))); }
  if (kind === "bundle") {
    delete x.subject; delete x.evidence;
    x.subject_digest = L.subject && L.subject.digest;
    x.evidence = Array.isArray(L.evidence) ? L.evidence.map((e) => (e && typeof e === "object" ? { digest: e.digest, v: e.v } : e)) : L.evidence;
    // in the port's order (v|canonical JSON, ascending), the references are put in the tool's (v|digest): the same order, read by each
    const key = (e, k) => `${e.v}|${k}`;
    if (Array.isArray(L.evidence) && L.evidence.every((e) => e && typeof e === "object") && L.evidence.every((e, i) => i === 0 || key(L.evidence[i - 1], canon(L.evidence[i - 1])) < key(e, canon(e))))
      x.evidence.sort((a, c) => (key(a, a.digest) < key(c, c.digest) ? -1 : 1));
    x.evidence_digest = sha(x.evidence);
  }
  x.digest = sha(x);
  return x;
}
// the tool's world IR made literal
function literalIR(ir) {
  const walk = (v, key) => {
    if (Array.isArray(v)) return v.map((x) => walk(x));
    if (!v || typeof v !== "object") return v;
    const o = {};
    for (const [k, x] of Object.entries(v)) {
      const L = typeof x === "string" ? lit.get(x) : undefined;
      if (k === "surface_digest") { if (!L) throw new Error("surface_digest"); o.seed = seedOf(L); }
      else if (k === "frame_digest") { if (!L) throw new Error("frame_digest"); o.frame = L; }
      else if (k === "packet_stream_digest") { o.packet_stream = L.packet_stream || L; }
      else if (k === "frame_stream_digest") { o.frame_stream = L; }
      else if (k === "digest" && key === "data") { if (!L) throw new Error("data digest"); }
      else if (k === "digest" && key === "intent") { if (!L) throw new Error("intent digest"); o[v.type === "wave31.hardware_decode_receipt.v0" ? "receipt" : "verify"] = L; }
      else if (k === "ref" && v.kind) { if (!L) throw new Error(`ref ${v.kind}`); o.ref = L; }
      else if (k === "kind" && typeof x === "string") o.kind = x.replace("_digest", "");
      else o[k] = walk(x, k);
    }
    return o;
  };
  return walk(ir);
}
// a stream digest names the rows: the receipt's packet stream is the uart rows, the verify's frame stream the emitter rows
function streams(C) { lit.set(shaText(nd(C.t.uart)), C.L.uart); lit.set(shaText(nd(C.t.emitter)), C.L.emitter); }

const MSG = [["surface_digest mismatch", "seed mismatch"], ["frame_digest mismatch", "frame mismatch"]];
const words = (s) => { for (const [a, b] of MSG) s = s.split(a).join(b); return s.replace(/ digest=sha256:[0-9a-f]{64}/g, ""); };

let n = 0;
const counts = {};
// files: name -> [literal, tool form] (or a string, both); the port is given the literal, the tool the other
function kase(name, files, args, opt = {}) {
  const d = `${OUT}/${name}${String(n++).padStart(3, "0")}`;
  mkdirSync(d, { recursive: true });
  const ta = [...args];
  for (const [f, v] of Object.entries(files)) {
    if (typeof v === "string") { writeFileSync(`${d}/${f}`, v); continue; }
    writeFileSync(`${d}/${f}`, v[0]);
    writeFileSync(`${d}/tool.${f}`, v[1]);
    const i = ta.indexOf(f); if (i >= 0) ta[i] = `tool.${f}`;
  }
  const oi = ta.indexOf("--out"); if (oi >= 0) ta[oi + 1] = `tool.${ta[oi + 1]}`;
  const p = run("handoff", ta, d);
  writeFileSync(`${d}/args`, args.join(" ") + "\n");
  writeFileSync(`${d}/expect.status`, `${p.status === 0 ? 0 : 2}\n`);
  writeFileSync(`${d}/expect.stdout`, words(p.stdout));
  writeFileSync(`${d}/expect.stderr`, words(p.stderr));
  if (p.status === 0 && oi >= 0) writeFileSync(`${d}/expect.${args[oi + 1]}`, canon(literalIR(read(`${d}/tool.${args[oi + 1]}`))));
  counts[name] = (counts[name] || 0) + 1;
  return p.status;
}
// an input as both are given it; its digests (and a stream's) named for the literal, for the world IR the tool makes of it
const J = (L, kind) => { const x = toolOf(kind, L); if (x && x.digest) lit.set(x.digest, L); return [canon(L), canon(x)]; };
const N = (rows, kind) => { const x = rows.map((r) => toolOf(kind, r)); x.forEach((r, i) => { if (r && r.digest) lit.set(r.digest, rows[i]); }); lit.set(shaText(nd(x)), rows); return [nd(rows), nd(x)]; };

// the bundles: the kit's golden and one of merge_review with its summary, each literal (in/mvevid's form)
function bundle(subject, ev, claim) {
  mkdirSync(W, { recursive: true });
  ev.forEach((e, i) => writeFileSync(`${W}/e${i}.json`, canon(e)));
  const p = run("bundle", ["build", "--subject-digest", subject.digest, "--claim-type", claim, ...ev.flatMap((_, i) => ["--evidence", `e${i}.json`]), "--out", "b.json"]);
  if (p.status !== 0) throw new Error(p.stderr);
  const b = read(`${W}/b.json`);
  const L = { authority: b.authority, claim_type: b.claim_type, evidence: [...ev].sort((a, c) => (a.v === c.v ? (canon(a) < canon(c) ? -1 : 1) : a.v.localeCompare(c.v))), subject, v: b.v };
  if (canon(toolOf("bundle", L).evidence.map((e) => e.digest).sort()) !== canon(b.evidence.map((e) => e.digest).sort())) throw new Error("bundle");
  lit.set(b.digest, L);
  return L;
}
const kd = (f) => read(`${KIT}/dev-docs/${f}`);
const SUBJECT = kd("wave17/merge-review.v0.json");
const KITB = bundle(SUBJECT, ["wave27/pointer-sync.residual.fail.v0.json", "wave28/signal-poly-projection.v0.json", "wave28/poly-decomp.v0.json", "wave29/action-plan.with-evidence.v0.json"].map(kd), "merge_review");
const MRB = bundle(SUBJECT, [kd("wave17/merge-review.v0.json"), kd("wave17/merge-review.summary.v0.golden.json")], "merge_review");
const ROLEB = bundle(SUBJECT, ["role.solon", "role.solomon", "role.metatron"].map((r) => kd(`wave18/${r}.v0.json`)), "roles");

// ── build-world-ir-wave30 ────────────────────────────────────────
const S = (p0, d, k) => ({ d: String(d), k_max: String(k), p0: String(p0) });
const chains = [chain(S(155, 79, 28), 12, "none"), chain(S(0, 1, 1), 1, "none"), chain(S(239, 119, 48), 30, "none"), chain(S(17, 7, 5), 7, "crc8-xor-v0"), chain(S(100, 113, 3), 12, "none")];
chains.forEach(streams);
for (const C of chains) {
  const base = { "surface.json": J(C.L.surface, "surface"), "frames.ndjson": N(C.L.frames, "frame") };
  const full = { ...base, "emitter.ndjson": N(C.L.emitter, "emitter"), "uart.ndjson": N(C.L.uart, "uart"), "receipt.json": J(C.L.receipt, "receipt"), "verify.json": J(C.L.verify, "verify") };
  kase("w", base, ["build-world-ir-wave30", "--surface", "surface.json", "--frames", "frames.ndjson", "--out", "world.json"]);
  kase("w", base, ["build-world-ir", "--surface", "surface.json", "--frames", "frames.ndjson", "--out", "world.json", "--world", "commons"]);
  kase("w", { ...base, "bundle.json": J(KITB, "bundle") }, ["build-world-ir-wave30", "--surface", "surface.json", "--frames", "frames.ndjson", "--bundle", "bundle.json", "--out", "world.json"]);
  kase("w", full, ["build-world-ir-wave30", "--surface", "surface.json", "--frames", "frames.ndjson", "--emitter", "emitter.ndjson", "--uart", "uart.ndjson", "--wave31-receipt", "receipt.json", "--wave31-frame-verify", "verify.json", "--bundle", "bundle.json", "--out", "world.json"].filter((a) => a !== "--bundle" && a !== "bundle.json"));
  kase("w", { ...full, "bundle.json": J(ROLEB, "bundle") }, ["build-world-ir-wave30", "--surface", "surface.json", "--frames", "frames.ndjson", "--emitter", "emitter.ndjson", "--uart", "uart.ndjson", "--bundle", "bundle.json", "--wave31-receipt", "receipt.json", "--wave31-frame-verify", "verify.json", "--out", "world.json", "--world", "x"]);
  kase("v", { "receipt.json": J(C.L.receipt, "receipt"), "verify.json": J(C.L.verify, "verify") }, ["build-world-ir-wave31", "--receipt", "receipt.json", "--frame-verify", "verify.json", "--out", "world.json"]);
}
// what the tool refuses of the chain
const C0 = chains[0], C1 = chains[4];
const base0 = { "surface.json": J(C0.L.surface, "surface"), "frames.ndjson": N(C0.L.frames, "frame") };
const wa = (...extra) => ["build-world-ir-wave30", "--surface", "surface.json", "--frames", "frames.ndjson", "--out", "world.json", ...extra];
kase("w", { ...base0, "bundle.json": J(MRB, "bundle") }, wa("--bundle", "bundle.json"));
kase("w", { ...base0, "emitter.ndjson": N(C0.L.emitter, "emitter"), "uart.ndjson": N(chains[3].L.uart, "uart") }, wa("--emitter", "emitter.ndjson", "--uart", "uart.ndjson"));
{ const crc = chains[3]; kase("w", { "surface.json": J(crc.L.surface, "surface"), "frames.ndjson": N(crc.L.frames, "frame"), "emitter.ndjson": N(crc.L.emitter, "emitter"), "uart.ndjson": N(crc.L.uart, "uart") }, wa("--emitter", "emitter.ndjson", "--uart", "uart.ndjson")); }
kase("w", { ...base0, "uart.ndjson": N(C0.L.uart, "uart") }, wa("--uart", "uart.ndjson"));
kase("w", { ...base0, "receipt.json": J(C0.L.receipt, "receipt") }, wa("--wave31-receipt", "receipt.json"));
kase("w", { ...base0, "receipt.json": J(C1.L.receipt, "receipt"), "verify.json": J(C1.L.verify, "verify") }, wa("--wave31-receipt", "receipt.json", "--wave31-frame-verify", "verify.json"));
kase("w", { "surface.json": J(C0.L.surface, "surface"), "frames.ndjson": N(C1.L.frames, "frame") }, wa());
kase("w", { ...base0, "emitter.ndjson": N(C1.L.emitter, "emitter") }, wa("--emitter", "emitter.ndjson"));
kase("w", { ...base0, "emitter.ndjson": N(C0.L.emitter.slice(0, 5), "emitter") }, wa("--emitter", "emitter.ndjson"));
kase("w", { ...base0, "emitter.ndjson": N(C0.L.emitter, "emitter"), "uart.ndjson": N(C0.L.uart.slice(0, 3), "uart") }, wa("--emitter", "emitter.ndjson", "--uart", "uart.ndjson"));
kase("w", { ...base0, "empty.ndjson": "\n" }, wa("--emitter", "empty.ndjson"));
kase("w", {}, ["build-world-ir-wave30", "--surface", "surface.json"]);
kase("w", {}, ["build-world-ir-wave31", "--receipt", "receipt.json"]);
kase("w", {}, ["verify-world-ir"]);
kase("w", {}, []);
kase("w", {}, ["--nope"]);
// each input's literal changed
const tamper = (what, kind, L, f, files, args) => { const x = clone(L); f(x); files[what] = Array.isArray(x) ? N(x, kind) : J(x, kind); kase("t", files, args); };
const full0 = () => ({ ...base0, "emitter.ndjson": N(C0.L.emitter, "emitter"), "uart.ndjson": N(C0.L.uart, "uart"), "receipt.json": J(C0.L.receipt, "receipt"), "verify.json": J(C0.L.verify, "verify"), "bundle.json": J(KITB, "bundle") });
const fa = wa("--emitter", "emitter.ndjson", "--uart", "uart.ndjson", "--wave31-receipt", "receipt.json", "--wave31-frame-verify", "verify.json", "--bundle", "bundle.json");
for (const f of [(s) => { s.v = "x"; }, (s) => { s.authority = "binding"; }, (s) => { s.ring_size = "241"; }, (s) => { s.extra = "1"; }, (s) => { delete s.chords; }])
  tamper("surface.json", "surface", C0.L.surface, f, full0(), fa);
for (const f of [(r) => { r[0].mode = "x"; }, (r) => { r[1].t = "5"; }, (r) => { r[1].chord_on = ["9", "3"]; }, (r) => { r[1].chord_on = ["3", "3"]; }, (r) => { r[1].chord_on = ["300"]; }, (r) => { r[1].chord_on = ["x1"]; }, (r) => { r[2].chord_dim = [...r[2].chord_on]; }, (r) => { r[2].seed = { ...r[2].seed, p0: "1" }; }, (r) => { r[0].extra = "x"; }, (r) => { r[0] = [1]; }, (r) => { r[3].pointer_on = "1"; }])
  tamper("frames.ndjson", "frame", C0.L.frames, f, full0(), fa);
for (const f of [(r) => { r[0].profile = "x"; }, (r) => { r[0].frame_ms = "0"; }, (r) => { r[1].pointer = ["1", "2"]; }, (r) => { r[1].pointer = [r[1].dim[0] || "7"]; }, (r) => { r[2].on = [...r[2].on, ...r[2].dim].sort((a, b) => a - b); }, (r) => { r[3].dim = []; }, (r) => { r[3].t = "4"; }])
  tamper("emitter.ndjson", "emitter", C0.L.emitter, f, full0(), fa);
for (const f of [(r) => { r[0].profile = "x"; }, (r) => { r[0].uart_crc = "crc16"; }, (r) => { r[0].packet_bytes = "66"; }, (r) => { r[0].packet_hex = r[0].packet_hex.toUpperCase(); }, (r) => { r[1].frame = r[2].frame; }, (r) => { r[1].seed = { ...r[1].seed, d: "1" }; }, (r) => { r[1].t = "9"; }])
  tamper("uart.ndjson", "uart", C0.L.uart, f, full0(), fa);
for (const f of [(x) => { x.v = "x"; }, (x) => { x.decode_profile_id = "x"; }, (x) => { x.uart_crc = "x"; }, (x) => { x.decode_ok = "2"; }, (x) => { x.error_count = "1"; }, (x) => { x.first_error_code = "e"; }, (x) => { x.decode_ok = "0"; x.error_count = "3"; x.first_error_code = "crc"; }, (x) => { x.packet_count = "x"; }, (x) => { x.seed = { ...x.seed, k_max: "2" }; }])
  tamper("receipt.json", "receipt", C0.L.receipt, f, full0(), fa);
for (const f of [(x) => { x.frame_type = "x"; }, (x) => { x.verify_ok = "0"; x.mismatch_count = "2"; x.first_mismatch_t = "3"; }, (x) => { x.verify_ok = "0"; x.first_mismatch_t = "none"; }, (x) => { x.mismatch_count = "1"; }, (x) => { x.frame_verify_id = "x"; }, (x) => { x.extra = "1"; }])
  tamper("verify.json", "verify", C0.L.verify, f, full0(), fa);
for (const f of [(b) => { b.v = "x"; }, (b) => { b.claim_type = ""; }, (b) => { b.evidence = []; }, (b) => { b.evidence.reverse(); }, (b) => { b.evidence.push(b.evidence[0]); }, (b) => { b.evidence[1].v = ""; }, (b) => { b.evidence[2] = "x"; }])
  tamper("bundle.json", "bundle", KITB, f, full0(), fa);
{ const files = { "receipt.json": J(C0.L.receipt, "receipt"), "verify.json": J(C1.L.verify, "verify") }; kase("t", files, ["build-world-ir-wave31", "--receipt", "receipt.json", "--frame-verify", "verify.json", "--out", "world.json"]); }

// ── verify-world-ir: the port's own world IRs, and changed ───────
const irs = [];
for (const C of chains.slice(0, 3)) {
  for (const a of [wa(), ["build-world-ir-wave31", "--receipt", "receipt.json", "--frame-verify", "verify.json", "--out", "world.json"]]) {
    const d = `${OUT}/.ir`; rmSync(d, { recursive: true, force: true }); mkdirSync(d, { recursive: true });
    const files = { "surface.json": toolOf("surface", C.L.surface), "frames.ndjson": null, "receipt.json": toolOf("receipt", C.L.receipt), "verify.json": toolOf("verify", C.L.verify) };
    for (const [f, v] of Object.entries(files)) writeFileSync(`${d}/${f}`, v ? canon(v) : nd(C.L.frames.map((r) => toolOf("frame", r))));
    if (run("handoff", a, d).status !== 0) throw new Error("ir");
    irs.push(literalIR(read(`${d}/world.json`)));
  }
}
rmSync(`${OUT}/.ir`, { recursive: true, force: true });
for (const ir of irs) kase("i", { "world.json": canon(ir) }, ["verify-world-ir", "--in", "world.json"]);
const ir0 = irs[0];
for (const f of [(x) => { x.world = ""; }, (x) => { x.extra = []; }, (x) => { x.entities = {}; }, (x) => { x.entities[1].id = x.entities[0].id; }, (x) => { x.entities[0].components = {}; }, (x) => { x.entities[0].owner = ""; }, (x) => { x.entities[0].zone = 3; }, (x) => { x.entities[0].components[0] = "x"; }, (x) => { x.entities[0].components[1].type = ""; }, (x) => { x.entities[0].components[1].data = []; }, (x) => { delete x.entities[0].components[1].data; },
  (x) => { x.zones = [{}]; }, (x) => { x.rules = {}; }, (x) => { x.events[0].id = ""; }, (x) => { x.events[0].intent = "x"; }, (x) => { x.events[0].authority = 1; }, (x) => { delete x.events[0].authority; }, (x) => { x.entities[0] = null; }, (x) => { x.portals = null; }, (x) => { x.entities[0].extra = "y"; }])
  { const x = clone(ir0); f(x); kase("i", { "world.json": canon(x) }, ["verify-world-ir", "--in", "world.json"]); }
rmSync(W, { recursive: true, force: true });
console.log(n, "cases:", JSON.stringify(counts));
