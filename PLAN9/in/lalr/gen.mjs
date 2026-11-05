// gen.mjs — the mutants for lalr.rc: from each of PLAN9/wsn/*.wsn, five files each one change away, every change made
// outside the literals: the first "." dropped, the first closing bracket dropped, the first "=" made "|", a "?" (a
// character no rule takes) after the first identifier, a literal's closing quote dropped. Every mutant must be
// rejected, and the readers (zwsn by recursive descent, wsn.y by Plan 9's yacc and by GNU Bison) must reject it on the
// same line.   node in/lalr/gen.mjs   (from PLAN9)
import { readdirSync, readFileSync, writeFileSync, mkdirSync, rmSync } from "node:fs";

const out = "in/lalr/mutants";
rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
// the positions outside literals, as zwsn reads them (a doubled quote inside a literal is one quote)
function outside(text) {
  const pos = [];
  for (let i = 0; i < text.length; i++) {
    if (text[i] === '"') {
      i++;
      for (; i < text.length && text[i] !== "\n"; i++) { if (text[i] === '"') { if (text[i + 1] === '"') i++; else break; } }
      continue;
    }
    pos.push(i);
  }
  return pos;
}
const closingQuote = (text) => {	// the closing quote of the first literal
  for (let i = 0; i < text.length; i++) if (text[i] === '"') {
    for (let j = i + 1; j < text.length && text[j] !== "\n"; j++) { if (text[j] === '"') { if (text[j + 1] === '"') j++; else return j; } }
  }
  return -1;
};
let n = 0;
for (const f of readdirSync("wsn").filter((x) => x.endsWith(".wsn")).sort()) {
  const t = readFileSync(`wsn/${f}`, "utf8"), pos = outside(t), base = f.replace(/\.wsn$/, "");
  const at = (pred) => pos.find((i) => pred(t[i]));
  const cut = (i) => t.slice(0, i) + t.slice(i + 1);
  const muts = {};
  let i = at((c) => c === "."); if (i !== undefined) muts.nodot = cut(i);
  i = at((c) => c === "]" || c === ")" || c === "}"); if (i !== undefined) muts.noclose = cut(i);
  i = at((c) => c === "="); if (i !== undefined) muts.eqbar = t.slice(0, i) + "|" + t.slice(i + 1);
  i = at((c) => /[A-Za-z]/.test(c)); if (i !== undefined) { let j = i; while (/[A-Za-z]/.test(t[j] ?? "")) j++; muts.badchar = t.slice(0, j) + "?" + t.slice(j); }
  i = closingQuote(t); if (i >= 0) muts.noquote = cut(i);
  for (const [k, v] of Object.entries(muts)) { writeFileSync(`${out}/${base}.${k}.wsn`, v); n++; }
}
writeFileSync(`${out}/LIST`, readdirSync(out).filter((x) => x.endsWith(".wsn")).sort().join("\n") + "\n");
console.log(`gen.mjs: ${n} mutants`);
