// verify.mjs — unpack the buffers Plan 9 wrote to run/out.img and compare each, byte for byte,
// with the buffer ZIGUAROTH's buffer.js wraps from the same file, path, type and description.
//
//   node verify.mjs [path/to/ZIGUAROTH]
//
// Plan 9's zbuf and the host's buffer.js are two compilers of one rule; a buffer passes only when
// they agree on every byte and buffer.js check accepts it.
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..");
const ZIG = process.argv[2] || "/home/main/Programs/ZIGUAROTH";
const BUFFER = join(ZIG, "_ARCHIVE/Expressions/buffer.js");
const OUT = join(HERE, "run/out");

rmSync(OUT, { recursive: true, force: true });
mkdirSync(OUT, { recursive: true });
execFileSync("tar", ["-xf", join(HERE, "run/out.img"), "-C", OUT]);

const types = Object.fromEntries(readFileSync(join(HERE, "src/TYPES"), "utf8").trim().split("\n").map((l) => l.split("\t")));
const files = readFileSync(join(HERE, "MANIFEST"), "utf8").trim().split("\n");
const fail = [];
for (const f of files) {
  const got = join(OUT, "zout", f + ".mime");
  if (!existsSync(got)) { fail.push(`${f}: no buffer`); continue; }
  const type = types[f.slice(f.lastIndexOf(".") + 1)] || "application/octet-stream";
  const description = `unbound/${f.split("/")[0]}, compiled in Plan 9 by zbuf`;
  const want = execFileSync("node", [BUFFER, "wrap", join(ROOT, f), `unbound/${f}`, type, description], { maxBuffer: 1 << 28 });
  const have = readFileSync(got);
  if (!have.equals(want)) {
    let i = 0;
    while (i < have.length && have[i] === want[i]) i++;
    fail.push(`${f}: differs from buffer.js at byte ${i} (${have.length} vs ${want.length} bytes)`);
    continue;
  }
  execFileSync("node", [BUFFER, "check", got]);
}
if (fail.length) {
  for (const f of fail) console.error(`FAIL: ${f}`);
  console.error(`FAIL: ${fail.length} of ${files.length}`);
  process.exit(1);
}
console.log(`PASS: ${files.length} buffers from Plan 9 equal buffer.js, byte for byte, and pass its check`);
