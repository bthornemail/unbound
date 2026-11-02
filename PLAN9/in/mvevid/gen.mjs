// gen.mjs - on the host, with Metaverse Kit's own tools mv-evidence-bundle and mv-alignment (node): each case
// its inputs, the port's arguments and what the port answers. The tools name what they hold by sha256: a
// bundle its subject and each evidence artifact (by the artifact's digest field), a report its four inputs (by
// the digest of each file's bytes), and each artifact itself. The port's are literal: a bundle holds its
// subject and its evidence artifacts whole, ordered by v as localeCompare orders it and, within a v, by their
// canonical JSON's code units; a report holds its four inputs whole, and validate recomputes their versions
// and the five checks. An emit's literal is read off the tool's own artifact (each digest checked against the
// literal it names); a validate's answer is the tool's own on the literal digested back (redigest; a bundle's
// references put in the tool's order), and for a report then the tool's own emit on the inputs it holds, save
// where a case says otherwise (an order or an evidence artifact the tool's form cannot see).
//	node gen.mjs <metaverse-kit> <out>
import { spawnSync } from "child_process";
import { writeFileSync, mkdirSync, readFileSync, rmSync, readdirSync, existsSync } from "fs";
import { createHash } from "crypto";
const [KIT, OUT] = process.argv.slice(2);
const T = { bundle: `${KIT}/tools/mv-evidence-bundle/index.js`, align: `${KIT}/tools/mv-alignment/index.js` };

function ss(v) { if (Array.isArray(v)) return `[${v.map(ss).join(",")}]`; if (v && typeof v === "object") { const ks = Object.keys(v).sort(); return `{${ks.map((k) => `${JSON.stringify(k)}:${ss(v[k])}`).join(",")}}`; } return JSON.stringify(v); }
const canon = (o) => `${ss(o)}\n`;
const sha = (o) => `sha256:${createHash("sha256").update(Buffer.from(canon(o), "utf8")).digest("hex")}`;
const clone = (o) => JSON.parse(JSON.stringify(o));
const read = (f) => JSON.parse(readFileSync(`${KIT}/${f}`, "utf8"));

// the bundle's literal order: v by localeCompare, then the canonical JSON's code units
const litOrder = (ev) => [...ev].sort((a, b) => (a.v === b.v ? (canon(a) < canon(b) ? -1 : canon(a) > canon(b) ? 1 : 0) : a.v.localeCompare(b.v)));
const toolOrder = (refs) => [...refs].sort((a, b) => (a.v === b.v ? a.digest.localeCompare(b.digest) : a.v.localeCompare(b.v)));
function redigest(kind, L) {
  const x = {};
  for (const [k, v] of Object.entries(L)) {
    if (kind === "bundle" && k === "subject") x.subject_digest = v.digest;
    else if (kind === "bundle" && k === "evidence") { x.evidence = Array.isArray(v) ? toolOrder(v.map((e) => (e && typeof e === "object" ? { digest: e.digest, v: e.v } : e))) : v; x.evidence_digest = sha(x.evidence); }
    else if (kind === "align" && ["narrative_state", "world_entities", "world_graph", "behavior_grammar"].includes(k)) x[`${k}_digest`] = sha(v);
    else x[k] = v;
  }
  x.digest = sha(x);
  return x;
}
function literal(kind, x, inp) {
  if (kind === "bundle") {
    const subject = inp.subject;
    if (x.subject_digest !== subject.digest) throw new Error("subject");
    const ev = x.evidence.map((r) => { const a = inp.evidence.find((e) => e.v === r.v && e.digest === r.digest); if (!a) throw new Error("evidence ref"); return a; });
    if (x.evidence_digest !== sha(x.evidence)) throw new Error("evidence_digest");
    return { authority: x.authority, claim_type: x.claim_type, evidence: litOrder(ev), subject, v: x.v };
  }
  const L = { authority: x.authority, checks: x.checks, projection_order: x.projection_order, status: x.status, v: x.v };
  for (const k of ["narrative_state", "world_entities", "world_graph", "behavior_grammar"]) { if (x[`${k}_digest`] !== sha(inp[k])) throw new Error(k); L[k] = inp[k]; }
  if (canon(redigest("align", L)) !== canon(x)) throw new Error("align: the literal does not digest back to the tool's artifact");
  return L;
}
const MSG = [["--subject-digest <sha256:...>", "--subject <subject.json>"], ["evidence artifact missing v/digest", "evidence artifact missing v"], ["build requires --subject-digest --claim-type --evidence... --out", "build requires --subject --claim-type --evidence... --out"]];
const words = (s) => { for (const [a, b] of MSG) s = s.split(a).join(b); return s.replace(/ digest=sha256:[0-9a-f]{64}/g, ""); };
const run = (kind, args, cwd) => spawnSync("node", [T[kind], ...args], { cwd, encoding: "utf8" });

let n = 0;
const counts = {};
function kase(kind, files, toolArgs, portArgs, opt = {}) {
  const d = `${OUT}/${kind[0]}${String(n++).padStart(3, "0")}`;
  mkdirSync(d, { recursive: true });
  for (const [f, v] of Object.entries(files)) writeFileSync(`${d}/${f}`, typeof v === "string" ? v : canon(v));
  const p = run(kind, toolArgs, d);
  writeFileSync(`${d}/args`, [kind, ...portArgs].join(" ") + "\n");
  let status = p.status, stdout = words(p.stdout), stderr = words(p.stderr), out = null;
  if (p.status === 0 && opt.emit) out = literal(kind, JSON.parse(readFileSync(`${d}/tool.out.json`, "utf8")), opt.emit);
  // a report validated: the tool's emit on the inputs the literal holds
  if (p.status === 0 && opt.recheck) {
    const e = `${d}/.re`; mkdirSync(e, { recursive: true });
    for (const k of ["narrative_state", "world_entities", "world_graph", "behavior_grammar"]) writeFileSync(`${e}/${k}.json`, canon(opt.recheck[k]));
    const q = run("align", ["emit", "--narrative-state", "narrative_state.json", "--world-entities", "world_entities.json", "--world-graph", "world_graph.json", "--behavior-grammar", "behavior_grammar.json", "--out", "o.json"], e);
    rmSync(e, { recursive: true });
    if (q.status !== 0) { writeFileSync(`${d}/tool.verdict`, `validate ${p.status} ${p.stdout}emit on the inputs held ${q.status} ${q.stderr}`); status = q.status; stdout = ""; stderr = q.stderr; }
  }
  if (opt.override) { writeFileSync(`${d}/tool.verdict`, `${p.status} ${p.stderr}${p.stdout}`); ({ status, stdout, stderr } = opt.override); out = opt.override.out || null; }
  writeFileSync(`${d}/expect.status`, `${status === 0 ? 0 : 2}\n`);
  writeFileSync(`${d}/expect.stdout`, stdout);
  writeFileSync(`${d}/expect.stderr`, stderr);
  if (out) writeFileSync(`${d}/expect.out`, canon(out));
  counts[kind] = (counts[kind] || 0) + 1;
  return { status, out };
}
const refuse = (msg) => ({ status: 2, stdout: "", stderr: `ERROR: ${msg}\n` });

let seed = 5;
const rnd = (k) => { seed = (seed * 1103515245 + 12345) % 2147483648; return Math.floor(seed / 65536) % k; };
const pick = (a) => a[rnd(a.length)];

// ── mv-evidence-bundle ───────────────────────────────────────────
const POOL = ["wave17/conflict-bundle.v0.json", "wave17/merge-review.summary.v0.golden.json", "wave17/shared-tape.v0.json", "wave18/dialogue-grammar.v0.json", "wave18/role.asabiyyah.v0.json", "wave18/role.metatron.v0.json", "wave18/role.solomon.v0.json", "wave18/role.solon.v0.json", "wave19/entity.v0.json", "wave19/world-entities.v0.json", "wave19/world-graph.v0.json", "wave20/behavior-grammar.v0.json", "wave21/alignment.v0.json", "wave21/world-graph.right.v0.json", "wave21/world-merge.v0.json", "wave22/reflection-result.v0.json", "wave23/archetype-signature.v0.json", "wave24/federation-merge.v0.json", "wave25/provider-extension.v0.json", "wave26/consumer-trace.v0.json", "wave27/pointer-sync.residual.fail.v0.json", "wave27/ring-basis.v0.json", "wave28/carrier-state.v0.json", "wave28/closed-config.v0.json", "wave28/constraints.v0.json", "wave28/poly-decomp.v0.json", "wave28/signal-poly-projection.v0.json", "wave28/zero-poly-basis.v0.json", "wave29/action-plan.projection-only.v0.json", "wave29/action-plan.with-evidence.v0.json"].map((f) => read(`dev-docs/${f}`));
const SUBJECT = read("dev-docs/wave17/merge-review.v0.json");
function bundleBuild(subject, claim, evidence, opt = {}) {
  const files = { "subject.json": subject };
  evidence.forEach((e, i) => { files[`e${i}.json`] = e; });
  const ev = evidence.flatMap((_, i) => ["--evidence", `e${i}.json`]);
  const c = claim === null ? [] : ["--claim-type", claim];
  return kase("bundle", files, ["build", "--subject-digest", subject.digest, ...c, ...ev, "--out", "tool.out.json"], ["build", "--subject", "subject.json", ...c, ...ev, "--out", "out.json"], { emit: { subject, evidence }, ...opt });
}
function bundleVerify(L, override) {
  return kase("bundle", { "bundle.json": L, "tool.bundle.json": redigest("bundle", L) }, ["verify", "--in", "tool.bundle.json"], ["verify", "--in", "bundle.json"], { override });
}
const KITEV = ["wave27/pointer-sync.residual.fail.v0.json", "wave28/signal-poly-projection.v0.json", "wave28/poly-decomp.v0.json", "wave29/action-plan.with-evidence.v0.json"].map((f) => read(`dev-docs/${f}`));
const bundles = [];
for (const ev of [KITEV, [...KITEV].reverse()]) { const r = bundleBuild(SUBJECT, "merge_review", ev); bundles.push(r.out); bundleVerify(r.out); }
const roles = POOL.filter((x) => x.v === "wave18.avatar_role.v0");
{ const r = bundleBuild(SUBJECT, "roles", [...roles].reverse()); bundles.push(r.out); bundleVerify(r.out); }
for (let i = 0; i < 30; i++) {
  const k = 1 + rnd(7), ev = [];
  for (let j = 0; j < k; j++) { const x = pick(POOL); if (!ev.includes(x)) ev.push(x); }
  const subj = pick([SUBJECT, ...POOL]);
  const r = bundleBuild(subj, pick(["merge_review", "witness", "名前", "a\"b", "x"]), ev);
  if (r.out) { bundles.push(r.out); bundleVerify(r.out); }
}
// the builds the tool refuses
bundleBuild(SUBJECT, "merge_review", [KITEV[0], KITEV[0]]);
bundleBuild(SUBJECT, null, KITEV);
bundleBuild(SUBJECT, "x", []);
{ const e = clone(KITEV[0]); e.authority = "binding"; bundleBuild(SUBJECT, "x", [e, KITEV[1]]); }
{ const e = clone(KITEV[1]); delete e.v; bundleBuild(SUBJECT, "x", [KITEV[0], e]); }
bundleBuild(SUBJECT, "x", [KITEV[0], '"a string"\n']);
kase("bundle", {}, ["build", "--bogus"], ["build", "--bogus"]);
kase("bundle", {}, [], []);
kase("bundle", {}, ["verify"], ["verify"]);
// what the tool's form cannot see: evidence with no digest of its own (the port's own literals), in the port's order
{
  const W = new URL("../mvwave/", import.meta.url).pathname, P = readdirSync(W).filter((d) => d[0] === "p" && existsSync(`${W}${d}/expect.out`)).sort()[0];	// the first provider literal of in/mvwave
  const own = [JSON.parse(readFileSync(`${W}${P}/expect.out`, "utf8")), KITEV[2]];
  const L = { authority: "advisory", claim_type: "literal", evidence: litOrder(own), subject: SUBJECT, v: "wave30.evidence_bundle.v0" };
  const files = { "subject.json": SUBJECT, "e0.json": own[0], "e1.json": own[1] };
  kase("bundle", files, ["build", "--subject-digest", SUBJECT.digest, "--claim-type", "literal", "--evidence", "e0.json", "--evidence", "e1.json", "--out", "tool.out.json"], ["build", "--subject", "subject.json", "--claim-type", "literal", "--evidence", "e0.json", "--evidence", "e1.json", "--out", "out.json"], { override: { status: 0, stdout: "ok mv-evidence-bundle build\n", stderr: "", out: L } });
  bundleVerify(L, { status: 0, stdout: "ok mv-evidence-bundle verify\n", stderr: "" });
}
// the verifies the tool refuses, each a literal changed
const b0 = bundles[0];
for (const f of [(b) => { b.authority = "binding"; }, (b) => { b.v = "wave30.evidence_bundle.v1"; }, (b) => { b.claim_type = ""; }, (b) => { b.claim_type = 3; }, (b) => { b.extra = "x"; }, (b) => { delete b.subject; }, (b) => { b.evidence = []; }, (b) => { b.evidence = {}; }, (b) => { b.evidence.push(clone(b.evidence[0])); b.evidence = litOrder(b.evidence); }])
  { const b = clone(b0); f(b); bundleVerify(b); }
// the order and the evidence, which the tool's form cannot see
{ const b = clone(b0); b.evidence.reverse(); bundleVerify(b, refuse("evidence_bundle evidence ordering mismatch")); }
{ const b = clone(bundles[2]); [b.evidence[0], b.evidence[1]] = [b.evidence[1], b.evidence[0]]; bundleVerify(b, refuse("evidence_bundle evidence ordering mismatch")); }
{ const b = clone(b0); b.evidence[1].authority = "binding"; bundleVerify(b, refuse("evidence[1] authority must be advisory")); }
{ const b = clone(b0); b.evidence[2].v = ""; b.evidence = [b.evidence[2], b.evidence[0], b.evidence[1], b.evidence[3]]; bundleVerify(b, refuse("evidence[0].v invalid")); }
{ const b = clone(b0); b.subject.reviewer = "changed"; bundleVerify(b); }	// the subject is held, not checked: the tool checks only its name

// ── mv-alignment ─────────────────────────────────────────────────
const N = read("dev-docs/narrative/states.v0.json"), E = read("dev-docs/wave19/world-entities.v0.json"), G = read("dev-docs/wave19/world-graph.v0.json"), B = read("dev-docs/wave20/behavior-grammar.v0.json");
function alignEmit(n, e, g, b, drop) {
  const f = { "narrative.json": n, "entities.json": e, "graph.json": g, "behavior.json": b };
  let a = ["--narrative-state", "narrative.json", "--world-entities", "entities.json", "--world-graph", "graph.json", "--behavior-grammar", "behavior.json"];
  if (drop) a = a.filter((_, i) => i !== drop && i !== drop + 1);
  return kase("align", f, ["emit", ...a, "--out", "tool.out.json"], ["emit", ...a, "--out", "out.json"], { emit: { narrative_state: n, world_entities: e, world_graph: g, behavior_grammar: b } });
}
function alignValidate(L, override) {
  const held = { narrative_state: L.narrative_state, world_entities: L.world_entities, world_graph: L.world_graph, behavior_grammar: L.behavior_grammar };
  const ok = Object.values(held).every((x) => x !== undefined);
  return kase("align", { "report.json": L, "tool.report.json": redigest("align", L) }, ["validate", "--alignment", "tool.report.json"], ["validate", "--alignment", "report.json"], { override, recheck: ok ? held : null });
}
const reports = [];
{ const r = alignEmit(N, E, G, B); reports.push(r.out); alignValidate(r.out); }
const nodes = ["a", "b", "c", "d"].map((c) => "sha256:" + c.repeat(64));
const VERBS = [["RELATE", "propose_relation"], ["UNRELATE", "propose_unrelation"], ["FOCUS_CLUSTER", "highlight"], ["TRACE_LINEAGE", "trace"], ["RELATE", "highlight"], ["UNRELATE", "propose_relation"]];
const ROLESET = ["solon", "solomon", "asabiyyah", "metatron", "witness", "asabiyah", "enoch"];
for (let i = 0; i < 40; i++) {
  const k = 2 + rnd(3), ns = nodes.slice(0, k);
  const e = clone(E); e.entities = ns.filter(() => rnd(14)).map((id, j) => ({ ...E.entities[0], node_id: id, x: String(j) }));
  const g = clone(G); g.relations = ns.slice(1).filter(() => rnd(14)).map((id, j) => ({ ...G.relations[0], source_node: ns[j], target_node: id }));
  const b = clone(B); b.rules = Array.from({ length: 1 + rnd(4) }, () => { const [verb, effect] = rnd(4) ? pick(VERBS.slice(0, 4)) : pick(VERBS); let s = rnd(k), t = rnd(k); if (rnd(3)) { if (s > t) [s, t] = [t, s]; if (s === t) t = Math.min(k - 1, s + 1); } return { ...B.rules[0], source_node: ns[s], target_node: ns[t], verb, effect }; });
  const n = clone(N); n.states = n.states.slice(0, 1 + rnd(3)).map((s) => ({ ...s, dialogue_roles: rnd(4) ? s.dialogue_roles : [pick(ROLESET), pick(ROLESET)] }));
  const r = alignEmit(n, e, g, b);
  if (r.out) { reports.push(r.out); alignValidate(r.out); }
}
for (const [w, f] of [["n", (x) => { x.v = "wave16.narrative_state.v1"; }], ["e", (x) => { x.v = "x"; }], ["g", (x) => { x.v = "wave19.world_graph.v9"; }], ["b", (x) => { x.v = "wave20.behavior_grammar.v1"; }]]) {
  const m = { n: clone(N), e: clone(E), g: clone(G), b: clone(B) }; f(m[w]); alignEmit(m.n, m.e, m.g, m.b);
}
{ const b = clone(B); delete b.rules; const n = clone(N); delete n.states; alignEmit(n, E, G, b); }
alignEmit(N, E, G, B, 6);
kase("align", {}, ["validate"], ["validate"]);
kase("align", {}, ["-h"], ["-h"]);
kase("align", {}, ["emit", "--x"], ["emit", "--x"]);
const r0 = reports[0];
for (const f of [(r) => { r.projection_order = ["world_graph", "world_entities", "narrative_state", "behavior_grammar"]; }, (r) => { r.status = "invalid"; }, (r) => { r.checks.acyclic_behavior_graph = "0"; }, (r) => { r.checks.acyclic_behavior_graph = "2"; }, (r) => { r.checks.extra = "1"; }, (r) => { r.authority = "binding"; }, (r) => { r.v = "wave21.alignment_report.v1"; }, (r) => { r.extra = "x"; }, (r) => { delete r.world_graph; }, (r) => { r.checks.narrative_roles_resolve = 1; },
  // what the tool's validate never sees, the inputs: each changed so a check fails, or its version
  (r) => { r.behavior_grammar.rules[0].target_node = "sha256:" + "0".repeat(64); }, (r) => { r.narrative_state.states[0].dialogue_roles[0] = "unknown_role"; }, (r) => { r.behavior_grammar.rules[1].source_node = r.behavior_grammar.rules[1].target_node; r.behavior_grammar.rules[1].target_node = r.behavior_grammar.rules[0].source_node; }, (r) => { r.behavior_grammar.rules[0].verb = "RELATE"; }, (r) => { r.world_entities.entities = []; }, (r) => { r.world_graph.v = "wave19.world_graph.v1"; }, (r) => { r.world_graph.relations[0].weight = "300"; }])
  { const r = clone(r0); f(r); alignValidate(r); }
console.log(n, "cases:", JSON.stringify(counts));
