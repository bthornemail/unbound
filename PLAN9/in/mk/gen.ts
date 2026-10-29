// gen.ts - on the host, with the kit's own code: write the event lines, the kit's state from
// them (shadow-canvas buildState), the ids in localeCompare order, and the must-reject lines
// (each one refused by the kit's validateWorldEvent). Run from the kit:
//	cd ~/devops/metaverse-kit && node_modules/.bin/tsx <PLAN9>/in/mk/gen.ts <PLAN9>/in/mk
import { writeFileSync } from "node:fs";
import { buildState } from "/home/main/devops/metaverse-kit/packages/shadow-canvas/src/index.ts";
import { validateWorldEvent, ROOT_INVARIANTS } from "/home/main/devops/metaverse-kit/packages/protocol/src/validate.ts";

const out = process.argv[2];
let seed = 0x1d1d;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff; return (seed >>> 16) % n; };
const pick = <T>(xs: T[]) => xs[rnd(xs.length)];

const IDCH = "_-:./0123456789abcABC".split("");
const NODES = ["n1", "N1", "n_2", "n-2", "n:3", "a", "A", "b0"];
const KEYS = ["color", "Size", "2", "10", "0", "x_y", "a-b"];
const VALS = ['"red"', "1", "2.5", "true", "null", "-3", '"Blue"'];
const RELS = ["parent", "connected_to", "near"];
const num = () => pick(["0", "1", "-2", "0.5", "3.25", "1e-7", "100"]);
const transform = () => ({ position: [num(), num(), num()].map(Number), rotation_quat: [num(), num(), num(), num()].map(Number), scale: [num(), num(), num()].map(Number) });
const props = () => { const p: Record<string, unknown> = {}; for (let i = rnd(4); i > 0; i--) p[pick(KEYS)] = JSON.parse(pick(VALS)); return p; };

const used = new Set<string>();
function eventId() {
  for (;;) { let s = ""; for (let i = 1 + rnd(4); i > 0; i--) s += pick(IDCH); if (!used.has(s)) { used.add(s); return s; } }
}

function envelope(operation: string) {
  return {
    event_id: eventId(), timestamp: 1000 + rnd(12) * 5, space_id: "demo", layer_id: pick(["layout", "physics", "presentation", "meta"]),
    actor_id: pick(["user:alice", "solver:phys_v1"]), operation,
    scope: rnd(3) ? { realm: pick(["personal", "team", "public"]), authority: pick(["source", "derived"]), boundary: pick(["interior", "boundary", "exterior"]) }
      : { realm: "team", authority: "source", boundary: "interior", policy: pick(["public", "private", "redacted"]) },
    preserves_invariants: [...ROOT_INVARIANTS], previous_events: [] as string[], tile: "z0/x0/y0",
  };
}

function event(): any {
  const op = pick(["create_node", "create_node", "update_transform", "set_properties", "set_properties", "link_nodes", "unlink_nodes", "delete_node",
    "set_geometry", "set_media", "set_text", "set_document", "physics_step", "merge", "set_physics", "macro.grow"]);
  const e: any = envelope(op);
  switch (op) {
    case "create_node": e.node_id = pick(NODES); e.kind = pick(["primitive.rectangle", "primitive.line"]); e.transform = transform(); if (rnd(2)) e.properties = props(); break;
    case "update_transform": e.node_id = pick(NODES); e.transform = transform(); break;
    case "set_properties": e.node_id = pick(NODES); e.properties = props(); break;
    case "link_nodes": case "unlink_nodes": e.from_node = pick(NODES); e.relation = pick(RELS); e.to_node = pick(NODES); break;
    case "delete_node": e.node_id = pick(NODES); break;
    case "set_geometry": e.node_id = pick(NODES); e.geometry = { kind: pick(["svg", "obj", "glb"]), ref: pick(["shapes/a.svg", "SOLOMON/shiba.glb"]) }; if (rnd(2)) e.geometry.units = pick(["m", "cm", "px"]); break;
    case "set_media": e.node_id = pick(NODES); e.media = { kind: pick(["svg", "wav", "mp4"]), ref: pick(["media/tone.wav", "media/a.mp4"]) }; if (rnd(2)) e.media.mime = pick(["audio/wav", "video/mp4"]); break;
    case "set_text": e.node_id = pick(NODES); e.text = { kind: pick(["plain", "markdown", "code"]), ref: pick(["hello", "text/readme.md"]) }; if (rnd(2)) e.text.language = pick(["en", "rc"]); break;
    case "set_document": e.node_id = pick(NODES); e.document = { kind: pick(["pdf", "md-page", "html"]), ref: pick(["docs/a.pdf", "docs/b.html"]) }; if (rnd(2)) e.document.pages = 1 + rnd(9); break;
    case "physics_step": e.solver = "phys_v1"; e.tick = rnd(100); e.delta_time = 0.5; e.updates = [{ node_id: pick(NODES), transform: transform() }, { node_id: pick(NODES), transform: transform() }]; break;
    case "merge": e.previous_events = [eventId(), eventId()]; break;
    case "set_physics": e.node_id = pick(NODES); e.physics = {}; break;
  }
  return e;
}

/** One event as one line: the envelope's fields in its order, each named by its own field name. */
const T = (t: any) => `transform ${[...t.position, ...t.rotation_quat, ...t.scale].map((x: unknown) => JSON.stringify(x)).join(" ")}`;
const P = (p: Record<string, unknown>) => ["properties", ...Object.entries(p).flatMap(([k, v]) => [k, JSON.stringify(v)])].join(" ");
const O = (name: string, o: Record<string, unknown>) => [name, ...Object.entries(o).flatMap(([k, v]) => [k, typeof v === "string" ? v : JSON.stringify(v)])].join(" ");
function line(e: any): string {
  const s = e.scope;
  const w = [`event_id ${e.event_id}`, `timestamp ${typeof e.timestamp === "number" ? JSON.stringify(e.timestamp) : e.timestamp}`, `space_id ${e.space_id}`, `layer_id ${e.layer_id}`,
    `actor_id ${e.actor_id}`, `operation ${e.operation}`, `scope ${s.realm} ${s.authority} ${s.boundary}` + ("policy" in s ? ` policy ${s.policy}` : ""),
    ["preserves_invariants", ...e.preserves_invariants].join(" "), ["previous_events", ...e.previous_events].join(" "), `tile ${e.tile}`];
  if ("node_id" in e) w.push(`node_id ${e.node_id}`);
  if ("kind" in e) w.push(`kind ${e.kind}`);
  if ("from_node" in e) w.push(`from_node ${e.from_node} relation ${e.relation} to_node ${e.to_node}`);
  if ("transform" in e) w.push(T(e.transform));
  if ("properties" in e) w.push(P(e.properties));
  for (const k of ["geometry", "media", "text", "document"]) if (k in e) w.push(O(k, e[k]));
  if ("solver" in e) w.push(`solver ${e.solver} tick ${e.tick} delta_time ${e.delta_time} updates ` + e.updates.map((u: any) => `node_id ${u.node_id} ${T(u.transform)}`).join(" "));
  return w.join(" ");
}

/** The kit's state, one line per node in the order the fold made them. */
function stateLines(events: any[]): string {
  const st = buildState("z0/x0/y0", null, events);
  const rows: string[] = [];
  for (const n of st.nodes.values()) {
    const w = [`node_id ${n.node_id} kind ${n.kind}` + (n.deleted ? " deleted" : ""), T(n.transform), P(n.properties), ["links", ...n.links.flatMap((l) => [l.relation, l.to])].join(" ")];
    for (const k of ["geometry", "media", "text", "document"] as const) if (n[k]) w.push(O(k, n[k] as any));
    rows.push(w.join(" "));
  }
  return rows.join("\n") + "\n";
}

const events = Array.from({ length: 400 }, event);
for (const e of events) validateWorldEvent(e);
// the lines are written before any fold: buildState keeps a create_node's properties object as the
// node's, so a later set_properties writes into the event itself; each fold gets its own copy
const lines = events.map(line);
const golden = stateLines(structuredClone(events));
const aliased = structuredClone(events); stateLines(aliased);
const written = aliased.filter((e, i) => line(e) !== lines[i]).length;
// the kit's claim, held here before the port is asked: the state does not depend on arrival order
if (stateLines(structuredClone(events).reverse()) !== golden) throw new Error("the kit's fold depends on arrival order");
writeFileSync(`${out}/events`, lines.join("\n") + "\n");
writeFileSync(`${out}/events.rev`, [...lines].reverse().join("\n") + "\n");
writeFileSync(`${out}/state`, golden);

const ids = [...used];
writeFileSync(`${out}/ids`, ids.join("\n") + "\n");
writeFileSync(`${out}/ids.sorted`, [...ids].sort((a, b) => a.localeCompare(b)).join("\n") + "\n");

// must-reject: each a valid event with one thing broken, refused by the kit before the grammar is asked
const breaks: [string, (e: any) => void][] = [
  ["an invariant missing", (e) => { e.preserves_invariants = ROOT_INVARIANTS.slice(1); }],
  ["realm not a realm", (e) => { e.scope.realm = "global"; }],
  ["authority not an authority", (e) => { e.scope.authority = "borrowed"; }],
  ["boundary not a boundary", (e) => { e.scope.boundary = "edge"; }],
  ["policy not a policy", (e) => { e.scope.policy = "secret"; }],
  ["timestamp not a number", (e) => { e.timestamp = "soon"; }],
  ["merge with one parent", (e) => { e.operation = "merge"; delete e.node_id; delete e.kind; delete e.transform; delete e.properties; e.previous_events = ["p1"]; }],
  ["position of two", (e) => { e.transform.position = [0, 0]; }],
  ["rotation of three", (e) => { e.transform.rotation_quat = [0, 0, 1]; }],
  ["create without a kind", (e) => { delete e.kind; }],
];
const reject: string[] = [];
for (const [why, f] of breaks) {
  const e = JSON.parse(JSON.stringify(events.find((x) => x.operation === "create_node")));
  f(e);
  try { validateWorldEvent(e); throw new Error("the kit accepts: " + why); } catch (err: any) { if (err.message.startsWith("the kit accepts")) throw err; }
  reject.push(line(e));
}
writeFileSync(`${out}/reject`, reject.join("\n") + "\n");
console.log(`${written} events written into by the fold; ${events.length} events, ${st(golden)} nodes, ${ids.length} ids, ${reject.length} must-reject`);
function st(s: string) { return s.trim().split("\n").length; }
