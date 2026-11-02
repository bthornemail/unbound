// gen.mjs - on the host, with Metaverse Kit's own tools/mv-portmatroid-project (node): each case a snapshot
// and the projection the tool writes of it. The real snapshots are port-matroid's own test fixtures, through
// the tool's own --snapshot path (it runs cabal run port-matroid-tool export-snapshot-json), kept as the
// JSON that path reads; the drawn ones are snapshots in the same shape, reaching what the projection writes
// as JavaScript writes it: numbers as Number's toString, a missing field as "undefined", components ?? {}.
//	node gen.mjs <metaverse-kit> <port-matroid> <out>
import { execFileSync, spawnSync } from "child_process";
import { readdirSync, writeFileSync, mkdirSync, readFileSync, statSync } from "fs";
import { join } from "path";
const [KIT, PM, OUT] = process.argv.slice(2);
const TOOL = `${KIT}/tools/mv-portmatroid-project/index.js`;
let n = 0, seed = 11;
const rnd = (k) => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % k; };
const pick = (a) => a[rnd(a.length)];
function kase(snapJson, args = [], viaCsnp = null) {
  const d = `${OUT}/m${String(n++).padStart(3, "0")}`; mkdirSync(d, { recursive: true });
  writeFileSync(`${d}/snap.json`, snapJson);
  const a = viaCsnp ? ["--snapshot", viaCsnp, "--pm-root", PM, ...args] : ["--snapshot-json", "snap.json", ...args];
  const p = spawnSync("node", [TOOL, ...a], { cwd: d, encoding: "utf8" });
  writeFileSync(`${d}/args`, ["--snapshot-json", "snap.json", ...args].join(" ") + "\n");
  writeFileSync(`${d}/expect.status`, `${p.status}\n`);
  writeFileSync(`${d}/expect.stdout`, p.stdout);
  writeFileSync(`${d}/expect.stderr`, (p.stderr.split("\n")[0] || "") + "\n");
  if (args.includes("--out")) writeFileSync(`${d}/expect.out`, readFileSync(`${d}/${args[args.indexOf("--out") + 1]}`));
}
// the real snapshots, through the tool's own csnp path
const real = [];
const walk = (dir) => readdirSync(dir).flatMap((f) => { const p = join(dir, f); return statSync(p).isDirectory() ? (f === "bad" ? [] : walk(p)) : f.endsWith(".csnp") ? [p] : []; });
for (const f of walk(`${PM}/test`).sort()) {
  const json = execFileSync("bash", ["-lc", `cd "${PM}" && cabal -v0 run port-matroid-tool -- export-snapshot-json "${f}"`], { encoding: "utf8" });
  kase(json, [], f); real.push(f);
}
// drawn snapshots
const nums = [0, 1, -1, 7, 1.5, 0.1, 2.5e-7, 1e21, 1e20, 123456789012, -0.0, 3.14159, 1e-6, 1e-7, 9007199254740993, 255, 65535];
const strs = ["test", "", "u64", "x\"y", "back\\slash", "line\nbreak", "tab\there", "\u0001ctl", "é", "名前", "αβγ", "𝔸 bold", "a b", "slash/"];
const value = (depth) => { const k = rnd(depth > 1 ? 5 : 7); return k === 0 ? pick(nums) : k === 1 ? pick(strs) : k === 2 ? rnd(2) === 1 : k === 3 ? null : k === 4 ? { t: pick(["u64", "string", "f64", "bool"]), v: String(pick(nums)) } : k === 5 ? [value(depth + 1), value(depth + 1)] : Object.fromEntries([["b", value(depth + 1)], ["a", value(depth + 1)], [pick(strs), value(depth + 1)]]); };
for (let i = 0; i < 40; i++) {
  const ents = [];
  for (let j = 0; j < rnd(6); j++) {
    const e = {};
    if (rnd(8)) e.eid = pick([j + 1, rnd(100000), String(rnd(1000)), "e-" + j, 1.5, true, null, [1, 2]]);
    if (rnd(8)) e.etype = pick(["test", "player", "tile", "名前", ""]);
    const comps = rnd(6);
    if (comps === 0) e.components = null;
    else if (comps > 1) e.components = Object.fromEntries(Array.from({ length: rnd(4) + 1 }, (_, c) => [pick(["_owner", "name", "pos", "Ω", "z", "A", "a"]) + (c ? c : ""), value(0)]));
    ents.push(e);
  }
  const snap = { tick: pick([0, 5, 1024, 1e21, 2.5, "7", null]), hash_hex: pick(["7453a6ea3cbe235c9980176ec21e0e54e8cd55522484391e9b6bbfa837b7c685", "abc", ""]), entity_count: ents.length, entities: ents };
  if (i % 9 === 4) delete snap.tick;
  if (i % 11 === 6) delete snap.hash_hex;
  if (i % 13 === 8) snap.entities = { not: "a list" };
  kase(JSON.stringify(snap), i % 5 === 2 ? ["--out", "proj.json"] : []);
}
// a JSON written with spaces, escapes and number forms JSON.parse reads and stringify rewrites
kase('{ "tick" : 1.0, "hash_hex":"\\u0041\\/b", "entity_count": 1e0, "entities": [ {"eid": 1.50, "etype":"\\u00e9\\ud835\\udd38", "components": {"n": -0, "big": 1E+21, "small": 0.000001, "z": [1.0, 2e2] } } ] }\n');
// the corners: keys sorted by UTF-16 code units (an astral key before U+FFFF), a lone surrogate, DEL, a
// number past double (Infinity: null in JSON, "Infinity" in String), String of arrays, objects, null, a
// duplicate key (the last wins), entities that are not objects
kase('{"tick":1e400,"hash_hex":"\\ud800x\u007f","entity_count":3,"entities":[{"eid":1e400,"etype":"t","components":{"a":1,"\\uffff":2,"\\ud83d\\ude00":3,"\\ue000":4}},{"eid":[1,[null,"x"],{}],"etype":"t","etype":"u","components":{"k":1,"k":2}},5,"abc",{"eid":{"a":1},"components":false},{"eid":-0,"components":0}]}\n');
// the numbers: 2^53 and its neighbours, the extremes, and 3000 drawn bit patterns, each as an eid, so the
// node id is Number's toString of the double (Plan 9's %e is not exact in the last digits)
{ const xs = [9007199254740992, 2 ** 53 + 2, 5e-324, 2.2250738585072014e-308, 1.7976931348623157e308, 0.1, 0.30000000000000004, 1 / 3, 1.23e-18, 1e21, 999999999999999900000, 1e-7, 123456.789];
  let s = 1; const b = new Float64Array(1), u = new Uint32Array(b.buffer);
  for (let i = 0; i < 3000; i++) { s = (s * 16807) % 2147483647; u[0] = s; s = (s * 16807) % 2147483647; u[1] = s % 0x7fefffff; xs.push(i % 2 ? b[0] : -b[0]); }
  kase(JSON.stringify({ tick: 0, hash_hex: "", entity_count: xs.length, entities: xs.map((x) => ({ eid: x, etype: "n" })) }) + "\n"); }
// the usage refusal
{ const d = `${OUT}/m${String(n++).padStart(3, "0")}`; mkdirSync(d, { recursive: true }); writeFileSync(`${d}/snap.json`, "{}\n");
  const p = spawnSync("node", [TOOL], { cwd: d, encoding: "utf8" });
  writeFileSync(`${d}/args`, "\n"); writeFileSync(`${d}/expect.status`, `${p.status}\n`); writeFileSync(`${d}/expect.stdout`, p.stdout); writeFileSync(`${d}/expect.stderr`, (p.stderr.split("\n")[0] || "") + "\n"); }
console.log(n, "cases,", real.length, "real snapshots");
