// gen.mjs - on the host, with Metaverse Kit's own tools mv-federate, mv-provider-metric and mv-consumer-metric
// (node): each case its inputs, the port's arguments, and what the port answers. The tools stamp each artifact
// with a sha256 of itself and name their inputs by digest; the port's artifacts are literal: each X_digest
// field holds, as X, the literal it stood for (left and right the world graphs, merged the merged relations,
// a conflict's left_relation and right_relation, a provider's canonical_world and extension, a trace's
// world), and the artifact's own digest is gone. An emit's literal is read off the tool's own artifact, each
// digest replaced by its literal and checked by digesting it back to the tool's bytes; a validate's answer is
// the tool's own on the literal digested back (redigest), its digest messages named for the literal, save
// where a case says otherwise (a forgery the digests cannot see: the literal changed, the digest it is named
// by kept).
//	node gen.mjs <metaverse-kit> <out>
import { spawnSync } from "child_process";
import { writeFileSync, mkdirSync, readFileSync, existsSync } from "fs";
import { createHash } from "crypto";
const [KIT, OUT] = process.argv.slice(2);
const T = { federate: `${KIT}/tools/mv-federate/index.js`, provider: `${KIT}/tools/mv-provider-metric/index.js`, consumer: `${KIT}/tools/mv-consumer-metric/index.js` };
const NAME = { federate: "mv-federate", provider: "mv-provider-metric", consumer: "mv-consumer-metric" };

// the tools' own canonical form and digest, as each writes them
function ss(v) { if (Array.isArray(v)) return `[${v.map(ss).join(",")}]`; if (v && typeof v === "object") { const ks = Object.keys(v).sort(); return `{${ks.map((k) => `${JSON.stringify(k)}:${ss(v[k])}`).join(",")}}`; } return JSON.stringify(v); }
const canon = (o) => `${ss(o)}\n`;
const sha = (o) => `sha256:${createHash("sha256").update(Buffer.from(canon(o), "utf8")).digest("hex")}`;
const clone = (o) => JSON.parse(JSON.stringify(o));

// literal <-> the tool's form
function redigest(kind, L) {
  const x = {};
  for (const [k, v] of Object.entries(L)) {
    if (kind === "federate" && (k === "left" || k === "right")) x[`${k}_digest`] = v.digest;
    else if (kind === "federate" && k === "merged") x.merged_digest = sha(v);
    else if (kind === "federate" && k === "conflict_summary" && Array.isArray(v))
      x[k] = v.map((c) => (c && typeof c === "object" && !Array.isArray(c) ? Object.fromEntries(Object.entries(c).map(([a, b]) => (a === "left_relation" || a === "right_relation" ? [`${a}_digest`, sha(b)] : [a, b]))) : c));
    else if (kind === "provider" && k === "canonical_world") x.canonical_world_digest = v.digest;
    else if (kind === "provider" && k === "extension") x.extension_digest = sha(v);
    else if (kind === "consumer" && k === "world") x.world_digest = v.digest;
    else x[k] = v;
  }
  x.digest = sha(x);
  return x;
}
function literal(kind, x, inp) {
  const L = clone(x);
  delete L.digest;
  const swap = (k, lit, test) => { if (!test) throw new Error(`${kind}: ${k} is not the literal's digest`); delete L[`${k}_digest`]; L[k] = lit; };
  if (kind === "federate") {
    swap("left", inp.left, x.left_digest === inp.left.digest);
    swap("right", inp.right, x.right_digest === inp.right.digest);
    swap("merged", { relations: x.merged_relations }, x.merged_digest === sha({ relations: x.merged_relations }));
    const last = (rels, id) => rels.filter((r) => r.relation_id === id).pop();
    L.conflict_summary = x.conflict_summary.map((c) => {
      const l = last(inp.left.relations, c.relation_id), r = last(inp.right.relations, c.relation_id);
      if (sha(l) !== c.left_relation_digest || sha(r) !== c.right_relation_digest) throw new Error("conflict digests");
      return { left_relation: l, relation_id: c.relation_id, resolution: c.resolution, right_relation: r };
    });
  }
  if (kind === "provider") {
    swap("canonical_world", inp.world, x.canonical_world_digest === inp.world.digest);
    const e = { behavior_extension_count: x.behavior_extension_count, extension_artifact_count: x.extension_artifact_count, extension_scope: x.extension_scope, projection_delta_size: x.projection_delta_size, provider_id: x.provider_id };
    swap("extension", e, x.extension_digest === sha(e));
  }
  if (kind === "consumer") swap("world", inp.world, x.world_digest === inp.world.digest);
  if (canon(redigest(kind, L)) !== canon(x)) throw new Error(`${kind}: the literal does not digest back to the tool's artifact`);
  return L;
}
// a tool's words for the literal: its digest messages named for what replaced them, its ok line without one
const MSG = [["input digest mismatch", "input literal mismatch"], ["merged_digest mismatch", "merged mismatch"], ["provider canonical_world_digest mismatch", "provider canonical_world mismatch"], ["canonical_world_digest mismatch", "canonical_world mismatch"], ["extension_digest mismatch", "extension mismatch"], ["ERROR: world_digest mismatch", "ERROR: world mismatch"]];
const words = (s) => { for (const [a, b] of MSG) s = s.split(a).join(b); return s.replace(/ digest=sha256:[0-9a-f]{64}/g, ""); };

let n = 0;
const counts = {};
function kase(kind, files, toolArgs, portArgs, opt = {}) {
  const d = `${OUT}/${kind[0]}${String(n++).padStart(3, "0")}`;
  mkdirSync(d, { recursive: true });
  for (const [f, v] of Object.entries(files)) writeFileSync(`${d}/${f}`, typeof v === "string" ? v : canon(v));
  const env = { ...process.env };
  delete env.WAVE26_Q_LOG_LEVEL;
  if (opt.env) env.WAVE26_Q_LOG_LEVEL = opt.env;
  const p = spawnSync("node", [T[kind], ...toolArgs], { cwd: d, encoding: "utf8", env });
  writeFileSync(`${d}/args`, [kind, ...portArgs].join(" ") + "\n");
  writeFileSync(`${d}/env`, (opt.env || "-") + "\n");
  let status = p.status, stdout = words(p.stdout), stderr = words(p.stderr);
  if (opt.override) { writeFileSync(`${d}/tool.verdict`, `${p.status} ${p.stderr}${p.stdout}`); ({ status, stdout, stderr } = opt.override); }
  writeFileSync(`${d}/expect.status`, `${status === 0 ? 0 : 2}\n`);
  writeFileSync(`${d}/expect.stdout`, stdout);
  writeFileSync(`${d}/expect.stderr`, stderr);
  let out = null;
  if (p.status === 0 && opt.emit) {
    out = literal(kind, JSON.parse(readFileSync(`${d}/tool.out.json`, "utf8")), opt.emit);
    writeFileSync(`${d}/expect.out`, canon(out));
  }
  counts[kind] = (counts[kind] || 0) + 1;
  return { status: p.status, out, dir: d };
}
const refuse = (kind, msg) => ({ status: 2, stdout: "", stderr: `ERROR: ${msg}\n` });

let seed = 7;
const rnd = (k) => { seed = (seed * 1103515245 + 12345) % 2147483648; return Math.floor(seed / 65536) % k; };
const pick = (a) => a[rnd(a.length)];
const hex = (c) => "sha256:" + c.repeat(64);
const rhex = () => "sha256:" + Array.from({ length: 64 }, () => "0123456789abcdef"[rnd(16)]).join("");

// ── mv-federate ──────────────────────────────────────────────────
const W24 = (f) => JSON.parse(readFileSync(`${KIT}/dev-docs/wave24/${f}`, "utf8"));
const kitL = W24("world-graph.left.v0.json"), kitR = W24("world-graph.right.v0.json");
function fedEmit(left, right, strategy, extra = []) {
  const s = strategy ? ["--strategy", strategy] : [];
  return kase("federate", { "left.json": left, "right.json": right }, ["emit", "--left", "left.json", "--right", "right.json", ...s, "--out", "tool.out.json", ...extra], ["emit", "--left", "left.json", "--right", "right.json", ...s, "--out", "out.json", ...extra], { emit: { left, right } });
}
function fedValidate(left, right, L, override) {
  return kase("federate", { "left.json": left, "right.json": right, "merge.json": L, "tool.merge.json": redigest("federate", L) }, ["validate", "--left", "left.json", "--right", "right.json", "--merge", "tool.merge.json"], ["validate", "--left", "left.json", "--right", "right.json", "--merge", "merge.json"], { override });
}
const fedLits = [];
for (const st of [null, "lexicographic", "left_preferred", "right_preferred"]) {
  const r = fedEmit(kitL, kitR, st);
  if (r.out) { fedValidate(kitL, kitR, r.out); fedLits.push([kitL, kitR, r.out]); }
}
const ids = ["0", "1", "2", "5", "7", "a", "c", "f"].map(hex);
const types = ["influences", "observes", "mirrors", "contains", "名前", "𝔸", "￿", "é"];
const stances = ["solon", "metatron", "solomon", "asabiyah", "enoch", "neutral"];
const rel = (id) => ({ relation_id: id, relation_type: pick(types), source_node: pick(ids), stance: pick(stances), target_node: pick(ids), weight: pick(["1", "2", "3", "5", "10", "0"]) });
const graph = (base, rels) => ({ authority: "advisory", base_world_entities_digest: base, digest: rhex(), relations: rels.sort((a, b) => a.relation_id.localeCompare(b.relation_id)), summary: { relation_count: String(rels.length), unique_node_count: String(new Set(rels.flatMap((r) => [r.source_node, r.target_node])).size) }, v: "wave19.world_graph.v0" });
const base = hex("8");
for (let i = 0; i < 30; i++) {
  const lr = ids.filter(() => rnd(2)).map(rel);
  const rr = [];
  for (const r of lr) { const k = rnd(3); if (k === 0) rr.push(clone(r)); if (k === 1) rr.push({ ...r, ...(rnd(2) ? { weight: pick(["4", "6", "1"]) } : { relation_type: pick(types) }) }); }
  for (const id of ids) if (!lr.some((r) => r.relation_id === id) && rnd(3) === 0) rr.push(rel(id));
  if (i % 10 === 3 && lr.length) { lr.push(clone(lr[0])); lr[lr.length - 1].weight = "99"; }	// a duplicate id: the last wins
  const L = graph(base, lr), R = graph(base, rr);
  const st = pick([null, "lexicographic", "left_preferred", "right_preferred"]);
  const r = fedEmit(L, R, st);
  if (r.out) { fedValidate(L, R, r.out); fedLits.push([L, R, r.out]); }
}
// the emits the tools refuse
{ const bad = clone(kitR); bad.base_world_entities_digest = hex("0"); fedEmit(kitL, bad, "lexicographic"); }
fedEmit(kitL, kitR, "bad");
kase("federate", { "left.json": kitL, "right.json": kitR }, ["emit", "--left", "left.json", "--right", "right.json"], ["emit", "--left", "left.json", "--right", "right.json"]);
kase("federate", { "left.json": kitL }, ["emit", "--left", "left.json"], ["emit", "--left", "left.json"]);
kase("federate", {}, ["emit", "--bogus"], ["emit", "--bogus"]);
kase("federate", {}, ["emit", "--left"], ["emit", "--left"]);
kase("federate", {}, [], []);
kase("federate", {}, ["--help"], ["--help"]);
kase("federate", { "left.json": kitL, "right.json": kitR }, ["validate", "--left", "left.json", "--right", "right.json"], ["validate", "--left", "left.json", "--right", "right.json"]);
for (const [what, f] of [
  ["version", (g) => { g.v = "wave19.world_graph.v1"; }], ["authority", (g) => { g.authority = "binding"; }], ["keyset", (g) => { g.extra = "x"; }],
  ["relations not a list", (g) => { g.relations = {}; }], ["unsorted", (g) => { g.relations.reverse(); }], ["a relation id", (g) => { g.relations[0].relation_id = "sha256:XYZ"; }],
  ["a relation's keys", (g) => { delete g.relations[0].stance; }], ["a target", (g) => { g.relations[1].target_node = hex("A"); }], ["the digest", (g) => { g.digest = "abc"; }],
  ["a null relation", (g) => { g.relations[0] = null; }], ["a number weight", (g) => { g.relations[1].weight = 1; }],
]) { const g = clone(kitR); f(g); fedEmit(kitL, g, "lexicographic"); }
{ const g = clone(kitL); g.relations[0].stance = 7; fedEmit(g, kitR, "left_preferred"); fedEmit(g, kitR, "right_preferred"); }
// the validates the tools refuse, each a literal changed
const [fl, fr, fm] = fedLits[0];
const fedT = [
  ["resolution", (m) => { m.conflict_summary[0].resolution = "bad"; }], ["a resolution relabelled", (m) => { m.conflict_summary[0].resolution = "left"; }],
  ["a merged weight", (m) => { m.merged_relations[0].weight = "9"; }], ["merged and its literal", (m) => { m.merged_relations[0].weight = "9"; m.merged.relations[0].weight = "9"; }],
  ["the strategy", (m) => { m.strategy = "right_preferred"; }], ["a strategy not one", (m) => { m.strategy = "random"; }],
  ["authority", (m) => { m.authority = "binding"; }], ["version", (m) => { m.v = "wave24.federation_merge_result.v1"; }], ["an extra key", (m) => { m.extra = "x"; }],
  ["a key gone", (m) => { delete m.merged; }], ["rejected", (m) => { m.rejected_components = ["x"]; }], ["no conflicts", (m) => { m.conflict_summary = []; }],
  ["unsorted", (m) => { m.merged_relations.reverse(); }], ["arrays", (m) => { m.conflict_summary = {}; }], ["a conflict's keys", (m) => { m.conflict_summary[0].extra = "x"; }],
  ["a conflict id", (m) => { m.conflict_summary[0].relation_id = "nope"; }], ["a conflict's relation", (m) => { m.conflict_summary[0].left_relation.weight = "8"; }],
  ["the left graph another", (m) => { m.left = clone(kitR); }], ["a number", (m) => { m.merged_relations[1].weight = 1; }],
];
for (const [, f] of fedT) { const m = clone(fm); f(m); fedValidate(fl, fr, m); }
// a strategy relabelled where nothing conflicts: the merge is the same under each, so it holds
{ const [l, r, m0] = fedLits.find(([, , m]) => m.conflict_summary.length === 0); const m = clone(m0); m.strategy = m.strategy === "left_preferred" ? "right_preferred" : "left_preferred"; fedValidate(l, r, m); }
// the forgery the digests cannot see: the left graph's literal changed, its digest field kept
{ const m = clone(fm); m.left.relations[0].weight = "300"; fedValidate(fl, fr, m, refuse("federate", "input literal mismatch")); }
{ const m = clone(fm); m.right.summary.relation_count = "30"; fedValidate(fl, fr, m, refuse("federate", "input literal mismatch")); }

// ── mv-provider-metric ───────────────────────────────────────────
const world = JSON.parse(readFileSync(`${KIT}/dev-docs/wave19/world-graph.v0.json`, "utf8"));
const pseed0 = JSON.parse(readFileSync(`${KIT}/dev-docs/wave25/provider.seed.json`, "utf8"));
const world2 = clone(kitR);
function provEmit(s, w = world) {
  return kase("provider", { "seed.json": s, "world.json": w }, ["emit", "--seed", "seed.json", "--world-graph", "world.json", "--out", "tool.out.json"], ["emit", "--seed", "seed.json", "--world-graph", "world.json", "--out", "out.json"], { emit: { world: w } });
}
function provValidate(L, w = world, override) {
  return kase("provider", { "world.json": w, "prov.json": L, "tool.prov.json": redigest("provider", L) }, ["validate", "--provider-extension", "tool.prov.json", "--world-graph", "world.json"], ["validate", "--provider-extension", "prov.json", "--world-graph", "world.json"], { override });
}
const nums = ["0", "1", "2", "4", "7", "000", "12", "0042", "9007199254740993", "99999999999999999999", "123456789012345678901234567890", "1" + "0".repeat(400), "-1", "1.5", "", "１", " 3", 7];
const provs = [];
{ const r = provEmit(pseed0); provs.push([r.out, world]); provValidate(r.out); }
{ const r = provEmit(pseed0, world2); provs.push([r.out, world2]); }
for (let i = 0; i < 30; i++) {
  const s = { behavior_extension_count: pick(nums.slice(0, 11)), extension_artifact_count: pick(nums.slice(0, 11)), extension_scope: pick(["renderer", "adapter", "projection", "tooling", "hybrid"]), projection_delta_size: pick(nums.slice(0, 11)), provider_id: pick(["mk.portal.renderer.v0", "p", "名前", "𝔸 provider", "a\"b"]) };
  if (i % 6 === 5) s[pick(["behavior_extension_count", "extension_artifact_count", "projection_delta_size"])] = pick(nums.slice(11));
  if (i === 7) s.projection_delta_size = "1" + "0".repeat(400);
  const r = provEmit(s);
  if (r.out) { provs.push([r.out, world]); provValidate(r.out); }
}
for (const f of [(s) => { s.extension_scope = "kernel"; }, (s) => { s.provider_id = ""; }, (s) => { s.provider_id = 3; }, (s) => { s.extra = "x"; }, (s) => { delete s.provider_id; }, (s) => { s.behavior_extension_count = 1; }]) { const s = clone(pseed0); f(s); provEmit(s); }
{ const w = clone(world); w.v = "wave19.world_graph.v9"; provEmit(pseed0, w); }
{ const w = clone(world); w.digest = "x"; provEmit(pseed0, w); }
kase("provider", { "seed.json": pseed0 }, ["emit", "--seed", "seed.json"], ["emit", "--seed", "seed.json"]);
kase("provider", { "seed.json": pseed0, "world.json": world }, ["emit", "--seed", "seed.json", "--world-graph", "world.json"], ["emit", "--seed", "seed.json", "--world-graph", "world.json"]);
kase("provider", { "world.json": world }, ["validate", "--world-graph", "world.json"], ["validate", "--world-graph", "world.json"]);
kase("provider", {}, ["-h"], ["-h"]);
kase("provider", {}, ["emit", "--nope", "x"], ["emit", "--nope", "x"]);
const pm = provs[0][0];
for (const f of [(x) => { x.magnitude_m = "8"; }, (x) => { x.magnitude_m = "seven"; }, (x) => { x.extension.provider_id = "other"; }, (x) => { x.provider_id = "other"; }, (x) => { x.provider_id = "other"; x.extension.provider_id = "other"; }, (x) => { x.provider_id = ""; x.extension.provider_id = ""; }, (x) => { x.extension_scope = "kernel"; }, (x) => { x.authority = "binding"; }, (x) => { x.v = "wave25.provider_extension.v1"; }, (x) => { x.canonical_world = clone(world2); }, (x) => { delete x.extension; }, (x) => { x.extra = "y"; }, (x) => { x.projection_delta_size = "5"; x.extension.projection_delta_size = "5"; x.magnitude_m = "8"; }])
  { const x = clone(pm); f(x); provValidate(x); }
provValidate(pm, world2);
{ const x = clone(pm); x.canonical_world.summary.relation_count = "9"; provValidate(x, world, refuse("provider", "canonical_world mismatch")); }

// ── mv-consumer-metric ───────────────────────────────────────────
const cseed0 = JSON.parse(readFileSync(`${KIT}/dev-docs/wave26/consumer.seed.json`, "utf8"));
function consEmit(s, prov, w = world, env) {
  return kase("consumer", { "seed.json": s, "world.json": w, "prov.json": prov, "tool.prov.json": redigest("provider", prov) }, ["emit", "--seed", "seed.json", "--world-graph", "world.json", "--provider-extension", "tool.prov.json", "--out", "tool.out.json"], ["emit", "--seed", "seed.json", "--world-graph", "world.json", "--provider-extension", "prov.json", "--out", "out.json"], { emit: { world: w }, env });
}
function consValidate(L, prov, w = world, env, override) {
  return kase("consumer", { "world.json": w, "prov.json": prov, "tool.prov.json": redigest("provider", prov), "trace.json": L, "tool.trace.json": redigest("consumer", L) }, ["validate", "--consumer-trace", "tool.trace.json", "--world-graph", "world.json", "--provider-extension", "tool.prov.json"], ["validate", "--consumer-trace", "trace.json", "--world-graph", "world.json", "--provider-extension", "prov.json"], { env, override });
}
const envs = [undefined, undefined, "WARN", "warn", "QUIET", "none", "DEBUG"];
const traces = [];
{ const r = consEmit(cseed0, pm); traces.push([r.out, pm]); consValidate(r.out, pm); }
for (let i = 0; i < 30; i++) {
  const [prov, w] = pick(provs.filter(([, w]) => w === world));
  const s = { behavior_invocations: pick(nums.slice(0, 11)), interaction_count: pick(nums.slice(0, 11)), invariant_axis_count: pick(nums.slice(0, 11)), max_projection_depth: pick(nums.slice(0, 11)) };
  if (i % 7 === 6) s[pick(Object.keys(s))] = pick(nums.slice(11));
  const env = pick(envs);
  const r = consEmit(s, prov, w, env);
  if (r.out) { traces.push([r.out, prov]); consValidate(r.out, prov, w, pick(envs)); }
}
consEmit(cseed0, provs[1][0]);	// a provider of another world
for (const f of [(s) => { s.extra = "x"; }, (s) => { s.interaction_count = "-3"; }, (s) => { delete s.max_projection_depth; }, (s) => { s.invariant_axis_count = 6; }]) { const s = clone(cseed0); f(s); consEmit(s, pm); }
{ const p = clone(pm); p.v = "wave25.provider_extension.v0x"; consEmit(cseed0, p); }
{ const p = clone(pm); p.extra = "x"; consEmit(cseed0, p); }
{ const p = clone(pm); p.magnitude_m = "7.0"; consEmit(cseed0, p); }
kase("consumer", { "world.json": world }, ["emit", "--world-graph", "world.json"], ["emit", "--world-graph", "world.json"]);
kase("consumer", { "world.json": world, "prov.json": pm, "tool.prov.json": redigest("provider", pm) }, ["validate", "--world-graph", "world.json", "--provider-extension", "tool.prov.json"], ["validate", "--world-graph", "world.json", "--provider-extension", "prov.json"]);
kase("consumer", {}, [], []);
const [tm, tp] = traces[0];
for (const f of [(t) => { t.q_value = "-91"; }, (t) => { t.q_value = "-9x"; }, (t) => { t.interaction_count = "10"; }, (t) => { t.provider_magnitude_m = "8"; }, (t) => { t.authority = "binding"; }, (t) => { t.v = "wave26.consumer_trace.v1"; }, (t) => { t.world = clone(world2); }, (t) => { t.extra = "z"; }, (t) => { delete t.q_value; }, (t) => { t.max_projection_depth = "4"; }, (t) => { t.max_projection_depth = 4; }])
  { const t = clone(tm); f(t); consValidate(t, tp); }
for (const env of envs.slice(2)) consValidate(tm, tp, world, env);
{ const t = clone(tm); t.world.relations[0].stance = "solomon"; consValidate(t, tp, world, undefined, refuse("consumer", "world mismatch")); }
console.log(n, "cases:", JSON.stringify(counts));
