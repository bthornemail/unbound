// lit.mjs - the literal artifact the port writes, from the tool's own artifact and inputs, with the
// tool's own library (tools/wave28-poly-lib.js): the digests taken out, each digest reference replaced
// by the literal it stood for, and the matrix the closed config only digested computed by the library.
//	node lit.mjs <kit> basis|constraints|carrier|closed|decomp|signal <tool artifact> [inputs...]
import { readFileSync } from "fs";
const [kit, kind, file, ...inputs] = process.argv.slice(2);
const L = await import(`${kit}/tools/wave28-poly-lib.js`);
const read = (f) => JSON.parse(readFileSync(f, "utf8"));
const strip = (o) => { const x = { ...o }; for (const k of Object.keys(x)) if (k === "digest" || k.endsWith("_digest")) delete x[k]; return x; };
const x = read(file);
let out;
if (kind === "basis" || kind === "constraints" || kind === "carrier") out = strip(x);
if (kind === "closed") {	// inputs: basis constraints carrier
  const [b, c, k] = inputs.map(read);
  const rows = c.row_masks.map((r, i) => L.xorBits6(r, L.rotateBits6(k.carrier_bits, i)));
  out = { ...strip(x), basis: b.basis, row_masks: c.row_masks, carrier_bits: k.carrier_bits, matrix: { columns: ["x1", "x2", "x3", "x4", "x5", "x6"], layout: L.MATRIX_LAYOUT_ID, rows } };
}
if (kind === "decomp") {	// inputs: basis closed(literal)
  const [b, cl] = inputs.map(read);
  out = { ...strip(x), basis: b.basis, closed_config: cl };
}
if (kind === "signal") {	// inputs: source path, decomp(literal) or ""
  const [source, decomp] = inputs;
  out = { ...strip(x), source, poly_decomp: decomp ? read(decomp) : "" };
}
process.stdout.write(L.canonicalJson(out));
