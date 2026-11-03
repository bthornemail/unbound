// gen.mjs - on the host: the goldens of zpixelate, PIXELATE as the author gives it, "both together its like the pi
// being resolved from the inceide in the omi pi proof, its casual": a polynomial's pixels (the 64 points of x1 to
// x6, each term lighting its light cone, the points at or above it: causal, a pixel only from the terms below it) and
// its spectrum (the binary FFT, Walsh-Hadamard: bin a is 64 - 2 x the points where the pixels and the linear function
// a.x differ, its signs parity phases, as the omi pi proof's terms take their sign from the phase's parity, pi
// entering only at the projection). Each line of polys, as the kit's library reads it: written, its line
// "pixels<TAB>bins"; refused, the reason as OMI.Poly names it (not written, out of order, repeated). The falsifier,
// zpixelate -n: the cones taken downward (the past cone, each pixel from the terms at or above it); the lines it
// gives filed here too, and how many differ.
//	node gen.mjs <metaverse-kit> <in/polywsn> <out>
import { readFileSync, writeFileSync } from "fs";
const [KIT, PW, OUT] = process.argv.slice(2);
const lib = await import(`${KIT}/tools/wave28-poly-lib.js`);
const read = (p) => {
  const [exit, err] = [process.exit, console.error];
  let msg = "";
  process.exit = () => { throw new Error("refused"); };
  console.error = (m) => { msg = String(m); };
  try { return { masks: lib.parsePolyCanonical(p, "p") }; }
  catch { return { why: /duplicate/.test(msg) ? "repeated" : /canonical order/.test(msg) ? "out of order" : "not written" }; }
  finally { [process.exit, console.error] = [exit, err]; }
};
const butterfly = (masks, down) => {
  const f = Array(64).fill(0);
  for (const t of masks) f[t] ^= 1;
  for (let i = 0; i < 6; i++) for (let p = 0; p < 64; p++) if (((p >> i) & 1) !== (down ? 1 : 0)) f[p] ^= f[p ^ (1 << i)];
  return f;
};
const par = (v) => { let k = 0; for (; v; v &= v - 1) k ^= 1; return k; };
const line = (masks, down) => {
  const f = butterfly(masks, down);
  const bins = [...Array(64).keys()].map((a) => 64 - 2 * f.filter((v, x) => v !== par(a & x)).length);
  return `${f.join("")}\t${bins.join(" ")}`;
};
const polys = ["yes", "no", "rules"].flatMap((k) => readFileSync(`${PW}/${k}`, "utf8").trim().split("\n"));
const up = [], down = [];
let differ = 0;
for (const p of polys) {
  const r = read(p);
  if (r.why) { up.push(r.why); down.push(r.why); continue; }
  up.push(line(r.masks, false)); down.push(line(r.masks, true));
  if (up.at(-1) !== down.at(-1)) differ++;
}
writeFileSync(`${OUT}/polys`, polys.map((p) => p + "\n").join(""));
writeFileSync(`${OUT}/cast`, up.map((l) => l + "\n").join(""));
writeFileSync(`${OUT}/cast.n`, down.map((l) => l + "\n").join(""));
const n = (w) => up.filter((l) => l === w).length;
console.log(polys.length, "lines:", up.length - n("not written") - n("out of order") - n("repeated"), "written,", n("not written"), "not written,", n("out of order"), "out of order,", n("repeated"), "repeated; the past cone differs on", differ);
