// gen.mjs - on the host, with Metaverse Kit's own tools/mv-universe-ingest (node): each case its inputs, the
// port's arguments and what the port answers. The tool builds a canonical package of a payload (its bytes held,
// named by a sha3 payload digest and package digest), carries a package as an Aztec bundle (zlib level 9,
// base64url, cut into chunks, each naming its bundle by a hash id and itself by a digest), ingests a package, a
// bundle, a universe XML, an ASG or a media reference into a scene, and reports readiness over five fixtures.
// The port is literal (the author's choices, 2026-11-02: "The package, held"; "Compressed, checked by round
// trip"): no hash anywhere; a package keeps its payload bytes and the payload's own claims (its digest,
// replay_hash, math_id_v2, as written); a bundle and its chunks are named by the package's name, "package:" and
// its law_version; a scene's package node is that name, a media node "media:" and its uri, and the scene holds
// its source (the text read) and its canonical form (the package, the XML text, the ASG's nodes and edges, the
// media reference) where the tool holds their digests, and the path as given where the tool holds the host's
// absolute path. An output's literal is read off the tool's own (each digest checked against the literal it
// names); an input's answer is the tool's own on the literal digested back. A bundle the port builds is
// compressed by libflate, whose bytes are not zlib's (checked in the guest, 2026-11-02: the kit's package at
// level 9 is 789 bytes by libflate, 796 by zlib, which inflates libflate's back to the same bytes), so a build
// is checked by its manifest and by ingesting it again; the literal bundles given to the port are made by zlib,
// the kit's own call (zlib.deflateSync, level 9).
//	node gen.mjs <metaverse-kit> <out>
import { spawnSync } from "child_process";
import { writeFileSync, mkdirSync, readFileSync, rmSync, readdirSync, cpSync } from "fs";
import { createHash } from "crypto";
import zlib from "zlib";
const [KIT, OUT] = process.argv.slice(2);
const TOOL = `${KIT}/tools/mv-universe-ingest/index.js`;
const FIX = `${KIT}/fixtures/universe-integration`;

function ss(v) { if (Array.isArray(v)) return `[${v.map(ss).join(",")}]`; if (v && typeof v === "object") { const ks = Object.keys(v).sort(); return `{${ks.map((k) => `${JSON.stringify(k)}:${ss(v[k])}`).join(",")}}`; } return JSON.stringify(v); }
const canon = (o) => `${ss(o)}\n`;
const H = (buf, algo) => (algo === "sha256" ? `sha256:${createHash("sha256").update(buf).digest("hex")}` : `sha3_256:${createHash("sha3-256").update(buf).digest("hex")}`);
const clone = (o) => JSON.parse(JSON.stringify(o));
const read = (f) => JSON.parse(readFileSync(f, "utf8"));
const W = `${OUT}/.w`;
const run = (args, cwd = W) => spawnSync("node", [TOOL, ...args], { cwd, encoding: "utf8" });
const b64u = (buf) => buf.toString("base64").replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");

// ── the package: literal <-> the tool's form ──
function litPackage(x) {	// the tool's package, its hashes checked and taken out
  const L = clone(x), algo = x.identity.hash_algo;
  if (x.identity.payload_digest !== H(Buffer.from(x.payload_bytes), algo)) throw new Error("payload_digest");
  const seed = clone(x); seed.identity.package_digest = "";
  if (x.identity.package_digest !== H(Buffer.from(canon(seed)), algo)) throw new Error("package_digest");
  delete L.manifest.hash_algo; delete L.identity.hash_algo; delete L.identity.payload_digest; delete L.identity.package_digest;
  return L;
}
function toolPackage(L, algo = "sha3_256") {	// the literal digested back
  if (!L || typeof L !== "object" || Array.isArray(L)) return L;
  const x = clone(L);
  if (x.manifest && typeof x.manifest === "object") x.manifest.hash_algo = algo;
  if (x.identity && typeof x.identity === "object") {
    x.identity.hash_algo = algo;
    let bytes = Buffer.alloc(0); try { bytes = Buffer.from(x.payload_bytes); } catch {}
    x.identity.payload_digest = H(bytes, algo);
    x.identity.package_digest = "";
    x.identity.package_digest = H(Buffer.from(canon(x)), algo);
  }
  return x;
}
const nameOf = (L) => `package:${L.manifest.law_version}`;
// a literal bundle, by zlib (the kit's own call); and the tool's bundle of the same package, by the tool
function litAztec(L, n) {
  const bytes = Buffer.from(canon(L)), enc = b64u(zlib.deflateSync(bytes, { level: 9 }));
  const parts = []; for (let i = 0; i < enc.length; i += n) parts.push(enc.slice(i, i + n));
  if (!parts.length) parts.push("");
  const bundle = nameOf(L);
  const manifest = { authority: "advisory", bundle, chunk_bytes: n, package_bytes: bytes.length, package_type: "ak.canonical.package.v2", payload_encoding: "zlib+base64url+package-bytes-v2", total_chunks: parts.length, type: "ak.aztec.bundle.v2", v: "ak.spec.v2" };
  return { manifest, chunks: parts.map((data, index) => ({ authority: "advisory", bundle, data, index, total: parts.length, type: "ak.aztec.chunk.v2" })) };
}
function writeAztec(dir, A) {
  mkdirSync(`${dir}/chunks`, { recursive: true });
  writeFileSync(`${dir}/manifest.json`, canon(A.manifest));
  A.chunks.forEach((c, i) => writeFileSync(`${dir}/chunks/chunk-${String(i).padStart(4, "0")}.json`, canon(c)));	// by place, so a changed index is seen
}
function readAztec(dir) { return { manifest: read(`${dir}/manifest.json`), chunks: readdirSync(`${dir}/chunks`).sort().map((f) => read(`${dir}/chunks/${f}`)) }; }
function redigestAztec(A, algo = "sha3_256") {	// a tool's bundle changed: its manifest and chunk digests made again
  const m = clone(A.manifest); delete m.manifest_digest; m.manifest_digest = H(Buffer.from(canon(m)), algo);
  return { manifest: m, chunks: A.chunks.map((c) => { const x = clone(c); delete x.chunk_digest; x.chunk_digest = H(Buffer.from(canon(x)), algo); return x; }) };
}

// a scene of the tool made literal: source as the port read it, the canonical form, the path as given, names for hashes
function litScene(x, o) {
  const s = clone(x); delete s.digest;
  if (o.toolRaw !== undefined && x.artifact_ref.source_digest !== H(Buffer.from(o.toolRaw), "sha256")) throw new Error("source_digest");
  s.artifact_ref = { canonical: o.canonical, source: o.raw, source_path: o.path };
  const rename = (id) => (id === o.from ? o.to : id);
  if (o.from !== undefined) { s.nodes = s.nodes.map((n) => ({ ...n, node_id: rename(n.node_id) })); s.projection_handles = s.projection_handles.map((p) => ({ ...p, node_id: rename(p.node_id) })); }
  return s;
}
function canonicalOf(scene, type, raw, pkgL) {	// what the scene's canonical_digest named, checked
  const d = scene.artifact_ref.canonical_digest;
  if (type === "package_v2" || type === "aztec_bundle_v2") { if (d !== H(Buffer.from(canon(toolPackage(pkgL, "sha3_256"))), "sha3_256") && d !== scene.nodes[0].node_id) throw new Error("package canonical"); return pkgL; }
  if (type === "xml_universe") { if (d !== H(Buffer.from(raw), "sha256")) throw new Error("xml canonical"); return raw; }
  if (type === "asg") { const c = { nodes: scene.nodes.map((n) => ({ id: n.node_id, label: n.label, type: n.node_type })), edges: scene.edges.map((e) => ({ from: e.from, to: e.to, relation: e.relation })) }; if (d !== H(Buffer.from(canon(c)), "sha256")) throw new Error("asg canonical"); return c; }
  if (type === "media_ref") { const r = JSON.parse(raw); if (d !== r.digest) throw new Error("media canonical"); return r; }
  throw new Error(type);
}
const MSG = [["aztec.chunk.bundle_id mismatch", "aztec.chunk.bundle mismatch"], ["aztec.compressed_digest mismatch", "aztec.compressed data invalid"],	// the port has the bytes, not their digest: a cut stream fails to inflate
   ["aztec.package_digest mismatch", "aztec.bundle mismatch"],
  [" [--hash-algo sha256|sha3_256]", ""]];
const words = (s, bundle) => { for (const [a, b] of MSG) s = s.split(a).join(b); s = s.replace(/ bundle_id=sha3?_?2?5?6?:?[0-9a-f]+/g, bundle ? ` bundle=${bundle}` : ""); return s.replace(/ digest=sha3?_?2?5?6?:[0-9a-f]{64}/g, ""); };

let n = 0;
const counts = {};
// a case: setup(d) writes the port's inputs (and the tool's, under tool/); the tool runs in tool/, the port in d
function kase(name, setup, args, opt = {}) {
  const d = `${OUT}/${name}${String(n++).padStart(3, "0")}`;
  mkdirSync(`${d}/tool`, { recursive: true });
  const ctx = setup(d) || {};
  const p = run(opt.toolArgs || args, `${d}/tool`);
  writeFileSync(`${d}/args`, args.join(" ") + "\n");
  let status = p.status, stdout = words(p.stdout, opt.bundle), stderr = words(p.stderr);
  if (opt.override) { writeFileSync(`${d}/tool.verdict`, `${p.status} ${p.stderr}${p.stdout}`); ({ status, stdout, stderr } = opt.override); }
  writeFileSync(`${d}/expect.status`, `${status === 0 ? 0 : 2}\n`);
  writeFileSync(`${d}/expect.stdout`, stdout);
  writeFileSync(`${d}/expect.stderr`, stderr);
  if (status === 0 && opt.out) for (const [f, v] of Object.entries(opt.out(d, ctx))) writeFileSync(`${d}/expect.${f.replace(/\//g, "@")}`, v);
  if (opt.then) {	// a second step: the port's own output ingested again
    writeFileSync(`${d}/then`, opt.then.args.join(" ") + "\n");
    for (const [f, v] of Object.entries(opt.then.expect)) writeFileSync(`${d}/expect2.${f.replace(/\//g, "@")}`, v);
  }
  counts[name] = (counts[name] || 0) + 1;
  return { status, d };
}

let rs = 13;
const rnd = (k) => { rs = (rs * 1103515245 + 12345) % 2147483648; return Math.floor(rs / 65536) % k; };
const pick = (a) => a[rnd(a.length)];

// ── build-package-v2 ─────────────────────────────────────────────
const payloads = [read(`${FIX}/sample-payload.json`)];
for (let i = 0; i < 14; i++) {
  const p = { mode: pick(["16d", "8t", "6t", "名前"]), law_version: pick(["16d-v1", "v2", "law/𝔸", "x"]), width: pick([32, 0, 1.5, 1e21, -7, 2.5e-7]), seed_hex: `0x${rnd(1 << 30).toString(16)}`, steps: rnd(9), states: Array.from({ length: rnd(4) }, (_, s) => ({ step: s, state_hex: `0x${rnd(1 << 30).toString(16).toUpperCase()}` })) };
  if (rnd(2)) p.digest = pick(["sha3_256:" + "1".repeat(64), "anything", 42]);
  if (rnd(2)) p.replay_hash = pick(["sha3_256:" + "2".repeat(64), { a: 1 }, ""]);
  if (rnd(3)) p.math_id_v2 = pick(["math_v2:4:abcd", "m"]);
  if (i % 5 === 4) p.note = "quote \" backslash \\ tab \t é";
  payloads.push(p);
}
const pkgs = [];
for (const [i, pl] of payloads.entries()) {
  const law = [null, "package-v2-draft", "16d-v1", "名前", "x_y"][i % 5];
  const la = law ? ["--law-version", law] : [];
  const algo = i % 3 === 2 ? ["--hash-algo", "sha256"] : [];
  const r = kase("p", (d) => { writeFileSync(`${d}/payload.json`, canon(pl)); writeFileSync(`${d}/tool/payload.json`, canon(pl)); }, ["build-package-v2", "--payload", "payload.json", "--out", "package.json", ...la], {
    toolArgs: ["build-package-v2", "--payload", "payload.json", "--out", "package.json", ...la, ...algo],
    out: (d) => { const L = litPackage(read(`${d}/tool/package.json`)); pkgs.push(L); return { "package.json": canon(L) }; } });
}
kase("p", () => {}, ["build-package-v2", "--out", "x.json"]);
kase("p", () => {}, []);
kase("p", () => {}, ["--bogus"]);
kase("p", () => {}, ["build-package-v2", "--payload", "p.json", "--out", "x.json", "--hash-algo", "sha256"], { override: { status: 2, stdout: "", stderr: "ERROR: unknown arg: --hash-algo\n" } });

// ── ingest: packages ─────────────────────────────────────────────
function ingestPackage(L, raw, opt = {}) {
  const x = toolPackage(L), toolRaw = canon(x);
  return kase("i", (d) => { writeFileSync(`${d}/package.json`, raw ?? canon(L)); writeFileSync(`${d}/tool/package.json`, toolRaw); }, ["ingest", "--in", "package.json", "--type", opt.type || "package", "--out", "scene.json"], {
    out: (d) => ({ "scene.json": canon(litScene(read(`${d}/tool/scene.json`), { toolRaw, raw: raw ?? canon(L), path: "package.json", canonical: L, from: x.identity && x.identity.package_digest, to: nameOf(L) })) }), ...opt });
}
for (const L of pkgs) ingestPackage(L, undefined, { type: rnd(2) ? "package" : "auto" });
const L0 = pkgs[0];
for (const f of [(x) => { x.v = "x"; }, (x) => { x.authority = "binding"; }, (x) => { x.extra = 1; }, (x) => { x.manifest.type = "x"; }, (x) => { x.manifest.spec_version = "x"; }, (x) => { x.manifest.section_order.reverse(); }, (x) => { x.manifest.extra = 1; },
  (x) => { delete x.control_plane.channels.FS; }, (x) => { x.control_plane.extra = 1; }, (x) => { x.identity.payload_encoding = "x"; }, (x) => { delete x.identity.source_math_id_v2; }, (x) => { x.payload_bytes = "x"; }, (x) => { x.payload_bytes[0] = 256; }, (x) => { x.payload_bytes[1] = 1.5; },
  (x) => { x.identity.payload_length += 1; }, (x) => { x.identity.payload_length = String(x.identity.payload_length); }, (x) => { x.payload_bytes = [...Buffer.from(canon({ other: "payload", law_version: "z" }))]; x.identity.payload_length = x.payload_bytes.length; }, (x) => { x.algorithms = "anything"; }])
  { const x = clone(L0); f(x); ingestPackage(x); }

// ── ingest: bundles ──────────────────────────────────────────────
function toolAztec(L, nb, d) {	// the tool's own bundle of the same package
  writeFileSync(`${W}/p.json`, canon(toolPackage(L)));
  rmSync(`${W}/az`, { recursive: true, force: true });
  const p = run(["build-aztec-v2", "--package", "p.json", "--outdir", "az", "--chunk-bytes", String(nb)]);
  if (p.status !== 0) throw new Error(p.stderr);
  return readAztec(`${W}/az`);
}
mkdirSync(W, { recursive: true });
function ingestAztec(L, nb, fl, ft, opt = {}) {	// fl, ft: the change to the literal bundle and to the tool's
  const A = litAztec(L, nb), T = toolAztec(L, nb);
  if (fl) fl(A); if (ft) ft(T);
  const T2 = ft ? redigestAztec(T) : T;
  const asFile = opt.file;
  return kase("a", (d) => { writeAztec(`${d}/bundle`, A); writeAztec(`${d}/tool/bundle`, T2); }, ["ingest", "--in", asFile ? "bundle/manifest.json" : "bundle", "--type", opt.type || "aztec", "--out", "scene.json"], {
    out: (d) => ({ "scene.json": canon(litScene(read(`${d}/tool/scene.json`), { raw: canon(A.manifest), path: asFile ? "bundle/manifest.json" : "bundle", canonical: L, from: T2.manifest.package_digest, to: nameOf(L) })) }) });
}
for (const [i, L] of pkgs.entries()) ingestAztec(L, [320, 1200, 64, 100000, 7][i % 5], null, null, { type: i % 4 === 1 ? "auto" : "aztec", file: i % 3 === 2 });
const KAZ = 320;
for (const [fl, ft] of [
  [(A) => { A.manifest.type = "x"; }, (T) => { T.manifest.type = "x"; }], [(A) => { A.manifest.authority = "x"; }, (T) => { T.manifest.authority = "x"; }],
  [(A) => { A.manifest.package_type = "x"; }, (T) => { T.manifest.package_type = "x"; }], [(A) => { A.manifest.extra = 1; }, (T) => { T.manifest.extra = 1; }],
  [(A) => { A.manifest.total_chunks += 1; }, (T) => { T.manifest.total_chunks += 1; }], [(A) => { A.chunks.pop(); A.manifest.total_chunks -= 1; A.chunks.forEach((c) => { c.total -= 1; }); }, (T) => { T.chunks.pop(); T.manifest.total_chunks -= 1; T.chunks.forEach((c) => { c.total -= 1; }); }],
  [(A) => { A.chunks[1].type = "x"; }, (T) => { T.chunks[1].type = "x"; }], [(A) => { A.chunks[1].authority = "x"; }, (T) => { T.chunks[1].authority = "x"; }],
  [(A) => { A.chunks[1].bundle = "package:other"; }, (T) => { T.chunks[1].bundle_id = "sha3_256:" + "9".repeat(64); }], [(A) => { A.chunks[1].total = 99; }, (T) => { T.chunks[1].total = 99; }],
  [(A) => { A.chunks[1].extra = 1; }, (T) => { T.chunks[1].extra = 1; }], [(A) => { A.chunks[2].index = 1; }, (T) => { T.chunks[2].index = 1; }],
  [(A) => { A.manifest.package_bytes += 1; }, (T) => { T.manifest.package_bytes += 1; }],
  [(A) => { A.manifest.bundle = "package:other"; A.chunks.forEach((c) => { c.bundle = "package:other"; }); }, (T) => { T.manifest.package_digest = "sha3_256:" + "8".repeat(64); }],
  [(A) => { A.chunks[0].data = A.chunks[0].data.slice(0, -4); }, (T) => { T.chunks[0].data = T.chunks[0].data.slice(0, -4); }]])
  ingestAztec(L0, KAZ, fl, ft);

// ── ingest: XML, ASG, media, the unknown ─────────────────────────
function ingestPlain(file, text, type, opt = {}) {
  return kase("x", (d) => { writeFileSync(`${d}/${file}`, text); writeFileSync(`${d}/tool/${file}`, text); }, ["ingest", "--in", file, "--type", type, "--out", "scene.json"], {
    out: (d) => { const s = read(`${d}/tool/scene.json`); const media = s.source_type === "media_ref" ? JSON.parse(text) : null;
      return { "scene.json": canon(litScene(s, { toolRaw: text, raw: text, path: file, canonical: canonicalOf(s, s.source_type, text), ...(media ? { from: `media:${media.digest}`, to: `media:${media.uri}` } : {}) })) }; } });
}
const xmls = [readFileSync(`${FIX}/sample-universe.xml`, "utf8"),
  `<universe>\n<node id="a"/>\n<node   id="b" label="" type="t"  />\n<edge from="a" to="b" relation="r"/>\n</universe>`,
  `  <universe id="u"><node id="a" label="A &amp; B"/><node id="c" x.y-z="1" label="C"/><edge from="c" to="a" relation="up" extra="1"/><edge from="a" to="a" relation="self"/></universe>  `,
  `<universe><node id="x" label="a>b"/><node id="y"/></universe>`,
  `<universe><nodes id="x"/><node id="y" type='single'/><node\tid="z"/></universe>`,
  `<universe><node label="no id"/></universe>`, `<universe><node id="a"/><node id="a"/></universe>`,
  `<universe><node id="a"/><edge from="a" to="b" relation="r"/></universe>`, `<universe><node id="a"/><edge from="a" to="a"/></universe>`,
  `<world><node id="a"/></world>`, `<universe><node id="a"/>`, `<universe></universe>`,
  `<universe><node id="名前" label="𝔸"/><node id="b" id="c"/><edge from="名前" to="c" relation="x"/></universe>`];
xmls.forEach((t, i) => ingestPlain(i % 2 ? "u.xml" : "universe.xml", t, i % 3 ? "auto" : "xml"));
const asgs = [readFileSync(`${FIX}/sample-asg.json`, "utf8"),
  canon({ nodes: [{ id: "a", label: 3 }, { id: "b", label: { x: 1 }, type: 7 }, { id: "c", label: [1, [2, null]], type: null }, { id: "d", label: 0 }, { id: "e", label: 1e21 }], edges: [{ from: "a", to: "b" }, { from: "c", to: "d", kind: 0 }, { from: "e", to: "a", kind: "K" }] }),
  canon({ nodes: [], edges: [] }), canon({ nodes: [{ id: "" }], edges: [] }), canon({ nodes: [{ id: 1 }], edges: [] }), canon({ nodes: [{ id: "a" }, { id: "a" }], edges: [] }),
  canon({ nodes: [{ id: "a" }], edges: [{ from: "a", to: "z" }] }), canon({ nodes: [{ id: "a" }], edges: [null] }), canon({ nodes: [null], edges: [] }), canon({ nodes: {}, edges: [] }), canon([1, 2]),
  canon({ nodes: [{ id: "a" }], edges: [{ from: "a" }] })];
asgs.forEach((t, i) => ingestPlain("asg.json", t, i % 2 ? "auto" : "asg"));
const medias = [readFileSync(`${FIX}/sample-media-ref.json`, "utf8"), ...["glb", "wav", "png", ""].map((k) => canon({ v: "media.ref.v0", authority: "advisory", media_kind: k, uri: `assets/a.${k || "x"}`, digest: "sha256:" + "4".repeat(64) })),
  canon({ v: "media.ref.v1", authority: "advisory", media_kind: "svg", uri: "u", digest: "d" }), canon({ v: "media.ref.v0", authority: "x", media_kind: "svg", uri: "u", digest: "d" }), canon({ v: "media.ref.v0", authority: "advisory", media_kind: "svg", uri: "u" }), canon({ v: "media.ref.v0", authority: "advisory", media_kind: "svg", uri: "", digest: "d" })];
medias.forEach((t, i) => ingestPlain("media.json", t, i % 2 ? "auto" : "media"));
ingestPlain("other.json", canon({ hello: "world" }), "auto");
ingestPlain("broken.json", "{not json", "auto");
ingestPlain("asg.json", asgs[0], "kml");
kase("x", () => {}, ["ingest", "--in", "x.json"]);

// ── build-aztec-v2, and the port's bundle ingested again ─────────
for (const [i, L] of pkgs.entries()) {
  const nb = [100000, 320, 50000, 64][i % 4], single = nb >= 10000;
  const T = toolAztec(L, nb);
  const A = litAztec(L, nb);
  const scene = (() => { rmSync(`${W}/s.json`, { force: true }); run(["ingest", "--in", "az", "--type", "aztec", "--out", "s.json"]); return litScene(read(`${W}/s.json`), { raw: canon(A.manifest), path: "out", canonical: L, from: T.manifest.package_digest, to: nameOf(L) }); })();
  const cb = nb === 1200 && i % 2 ? [] : ["--chunk-bytes", String(nb)];
  kase("z", (d) => { writeFileSync(`${d}/package.json`, canon(L)); writeFileSync(`${d}/tool/package.json`, canon(toolPackage(L))); }, ["build-aztec-v2", "--package", "package.json", "--outdir", "out", ...cb], {
    bundle: nameOf(L),
    out: () => (single ? { "out/manifest.json": canon(A.manifest) } : {}),
    then: { args: ["ingest", "--in", "out", "--type", "aztec", "--out", "scene.json"], expect: { status: "0\n", stdout: "ok mv-universe-ingest ingest\n", stderr: "", ...(single ? { "scene.json": canon(scene) } : {}) } } });
}
{ const x = clone(L0); x.v = "x"; kase("z", (d) => { writeFileSync(`${d}/package.json`, canon(x)); writeFileSync(`${d}/tool/package.json`, canon(toolPackage(x))); }, ["build-aztec-v2", "--package", "package.json", "--outdir", "out"]); }
kase("z", () => {}, ["build-aztec-v2", "--package", "p.json"]);

// ── readiness ────────────────────────────────────────────────────
function fixtures(dir, tdir, f = {}) {
  const L = litPackage(read(`${FIX}/sample-package.v2.json`));
  mkdirSync(dir, { recursive: true });
  cpSync(FIX, tdir, { recursive: true });
  writeFileSync(`${dir}/sample-package.v2.json`, canon(L));
  writeAztec(`${dir}/sample-aztec-v2`, litAztec(L, 320));
  for (const x of ["sample-universe.xml", "sample-asg.json", "sample-media-ref.json"]) cpSync(`${FIX}/${x}`, `${dir}/${x}`);
  for (const [name, text] of Object.entries(f)) { writeFileSync(`${dir}/${name}`, text); writeFileSync(`${tdir}/${name}`, text); }
}
const report = (d, out) => { const r = read(`${d}/tool/${out}`); delete r.digest; return { [out]: canon(r) }; };
kase("r", (d) => fixtures(`${d}/fx`, `${d}/tool/fx`), ["readiness", "--fixtures", "fx", "--out", "report.json"], { out: (d) => report(d, "report.json") });
kase("r", (d) => fixtures(`${d}/fixtures/universe-integration`, `${d}/tool/fixtures/universe-integration`), ["readiness", "--out", "reports/integration/readiness.json"], { out: (d) => report(d, "reports/integration/readiness.json") });
kase("r", (d) => fixtures(`${d}/fx`, `${d}/tool/fx`, { "sample-asg.json": canon({ nodes: {}, edges: [] }) }), ["readiness", "--fixtures", "fx", "--out", "report.json"]);
kase("r", (d) => fixtures(`${d}/fx`, `${d}/tool/fx`, { "sample-universe.xml": "<universe><node id=\"a\"/><node id=\"b\"/><edge from=\"a\" to=\"b\" relation=\"r\"/></universe>" }), ["readiness", "--fixtures", "fx", "--out", "report.json"], { out: (d) => report(d, "report.json") });
kase("r", () => {}, ["readiness"]);
rmSync(W, { recursive: true, force: true });
console.log(n, "cases:", JSON.stringify(counts));
