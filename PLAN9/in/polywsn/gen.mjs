// gen.mjs - on the host: the questions polywsn.rc asks of wsn/poly.wsn by zlazy, with Metaverse Kit's own
// tools/wave28-poly-lib.js as the reading beside it. yes: every polynomial written in in/mvpoly's literals
// (input_poly, residual_poly), every one of the 64 terms alone, the full polynomial (all 64), and 120 drawn ones,
// each written by the library's renderPolyFromMasks; the library reads each back to its masks (parsePolyCanonical).
// no: each written wrongly in a way the grammar refuses (a variable out of order or twice, x0 or x7, an empty term,
// "0" or "1" joined to a term), each refused by the library too. rules: each of the yes with two terms swapped or a
// term written twice, which the grammar accepts and the library refuses by poly.rules 1 and 2 (the order and the
// repeat a grammar does not say).
//	node gen.mjs <metaverse-kit> <in/mvpoly> <out>
import { readdirSync, readFileSync, writeFileSync } from "fs";
const [KIT, MV, OUT] = process.argv.slice(2);
const lib = await import(`${KIT}/tools/wave28-poly-lib.js`);
// the library refuses by die (its message, then process.exit(2)); read with the exit thrown and the message kept
const reads = (p) => {
  const [exit, err] = [process.exit, console.error];
  process.exit = () => { throw new Error("refused"); };
  console.error = () => {};
  try { lib.parsePolyCanonical(p, "p"); return true; } catch { return false; } finally { [process.exit, console.error] = [exit, err]; }
};
const yes = new Set();
for (const d of readdirSync(MV)) {
  let s = "";
  try { for (const f of readdirSync(`${MV}/${d}`)) s += readFileSync(`${MV}/${d}/${f}`, "utf8"); } catch { continue; }
  for (const m of s.matchAll(/"(?:input_poly|residual_poly)":"([^"]*)"/g)) yes.add(m[1]);
}
for (let m = 0; m < 64; m++) yes.add(lib.renderPolyFromMasks([m]));
yes.add(lib.renderPolyFromMasks([...Array(64).keys()]));
let seed = 0x2b;
const next = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) >> 8;
for (let i = 0; i < 120; i++) {
  const n = 1 + (next() % 12), set = new Set();
  while (set.size < n) set.add(next() % 64);
  yes.add(lib.renderPolyFromMasks([...set]));
}
const Y = [...yes].sort();
const wrong = (p) => {
  const out = [], terms = p === "0" ? [] : p.split("+");
  for (const [i, t] of terms.entries()) {
    const f = t.split("*");
    const put = (u) => out.push([...terms.slice(0, i), u, ...terms.slice(i + 1)].join("+"));
    if (f.length > 1) { put([f[1], f[0], ...f.slice(2)].join("*")); put([f[0], ...f].join("*")); }
    if (t !== "1") { put(t.replace(/x(\d)/, (_, d) => (d === "6" ? "x7" : "x0"))); put(`1*${t}`); }
  }
  if (terms.length > 1) out.push(terms.join("++"), "+" + p, p + "+");
  out.push(`0+${p === "0" ? "x1" : p}`);
  return out;
};
const N = [...new Set(Y.flatMap(wrong))].filter((p) => !yes.has(p)).sort();
const R = [...new Set(Y.flatMap((p) => {
  const t = p === "0" ? [] : p.split("+");
  return t.length > 1 ? [[t[1], t[0], ...t.slice(2)].join("+"), [t[0], ...t].join("+")] : t.length === 1 ? [`${t[0]}+${t[0]}`] : [];
}))].sort();
const bad = [...Y.filter((p) => !reads(p)).map((p) => "yes refused: " + p), ...N.filter(reads).map((p) => "no read: " + p), ...R.filter(reads).map((p) => "rules read: " + p)];
if (bad.length) throw new Error(bad.slice(0, 5).join("\n"));
writeFileSync(`${OUT}/yes`, Y.map((p) => p + "\n").join(""));
writeFileSync(`${OUT}/no`, N.map((p) => p + "\n").join(""));
writeFileSync(`${OUT}/rules`, R.map((p) => p + "\n").join(""));
console.log(Y.length, "written by the library and read back;", N.length, "written wrongly, refused by it;", R.length, "out of order or repeated, refused by it; the longest", Math.max(...Y.map((p) => p.length)), "characters");
