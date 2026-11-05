// gen.mjs - the cases for src/wordfs.rc and their expected answers, computed here apart from wordfs.c: for each path,
// whether it resolves and, if so, its qid (the word's ordinal, in the order wordfs first meets words) and its listing,
// the word's relation to YHWH and YAHWEH as Braille cells (XOR with the key once and repeated, and each XNOR), each
// once, sorted as ls sorts (bytes).	node in/wordfs/gen.mjs
import { writeFileSync } from 'node:fs';
const KEYS = ['YHWH', 'YAHWEH'], CELL = 0x2800;
const decode = (s) => {
  if (/[0-9]/.test(s)) return -2;
  const cps = [...s].map((c) => c.codePointAt(0));
  if (cps.length && cps.every((r) => r >= CELL && r <= CELL + 255)) return Buffer.from(cps.map((r) => r - CELL));
  return Buffer.from(s, 'utf8');
};
const forms = (b) => {
  const out = [];
  for (const key of KEYS) for (const xnor of [0, 1]) for (const again of [0, 1]) {
    const k = Buffer.from(key);
    out.push(String.fromCodePoint(...[...b].map((v, j) => CELL + ((v ^ (again ? k[j % k.length] : (k[j] ?? 0))) ^ (xnor ? 255 : 0)))));
  }
  // wordfs's order: per key XOR once, XOR repeated, XNOR once, XNOR repeated
  const ordered = [];
  for (let i = 0; i < 8; i++) { const key = Math.floor(i / 4), again = i % 2, xnor = Math.floor(i / 2) % 2; ordered.push(out[key * 4 + xnor * 2 + again]); }
  return [...new Set(ordered)];
};
const words = [];
const ordinal = (b) => { const i = words.findIndex((w) => w.equals(b)); if (i >= 0) return i + 1; words.push(b); return words.length; };
// Plan 9 cleans a path lexically before walking it (cleanname): name/.. goes
const clean = (path) => { const out = []; for (const s of path.split('/').filter(Boolean)) { if (s === '..') out.pop(); else out.push(s); } return out; };
function walk(path) {
  let b = null;
  for (const seg of clean(path)) {
    const d = decode(seg); if (d === -2 || d.length === 0) return null;
    b = b ? Buffer.concat([b, Buffer.from(' '), d]) : d;
    ordinal(b);
  }
  return b;
}
const cases = ['YHWH', 'YAHWEH', 'YAH', 'GOD/IS/WORD', 'GOD/OF/WORD', 'אלהים', 'יהוה', 'ΘΕΟΣ', 'one/point/two/repeating/three/four',
  'WORD', '⠀⠉⠀', '⢎⣑⢀⣛⣗⢢', 'GOD/..', '1', 'one/1', 'YHWH/YAHWEH'];
const lines = [];
for (const c of cases) {
  // test -d, ls -qd and ls -pQ each walk the path; ls -pQ then lists, meeting each listed word in order
  if (!clean(c).length) { lines.push(`${c}: ${'0'.repeat(16)}`); continue; }
  const b = walk(c); walk(c);
  if (!b) { lines.push(`${c}: refused`); continue; }
  const q = ordinal(b); walk(c);
  const fs = forms(b); for (const f of fs) ordinal(decode(f));
  const names = fs.slice().sort((x, y) => Buffer.compare(Buffer.from(x), Buffer.from(y)));
  lines.push(`${c}: ${q.toString(16).padStart(16, '0')} ${names.join(' ')}`);
}
writeFileSync(new URL('cases', import.meta.url), cases.join('\n') + '\n');
writeFileSync(new URL('expected', import.meta.url), lines.join('\n') + '\n');
console.log(lines.map((l) => l.slice(0, 110)).join('\n'));
