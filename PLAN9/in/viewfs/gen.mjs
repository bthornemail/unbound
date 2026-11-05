// gen.mjs - the inputs for src/viewfs.rc and each circuit's view of them, computed here apart from viewfs.c:
//	words	Genesis 1:1 in English, as the KJV has it (public domain), and the Name
//	bytes	the 256 byte values, then a sine of 64 eight-bit samples (binary data)
// expected/CIRCUIT.FILE (xor and xnor with the key: xor-YHWH.words ...): the view as viewfs must give it.
import { writeFileSync } from 'node:fs';
const here = (f) => new URL(f, import.meta.url);
const words = Buffer.from('In the beginning God created the heaven and the earth.\nYHWH YAHWEH YAH\n');
const bytes = Buffer.from([...Array(256).keys(), ...[...Array(64).keys()].map((i) => Math.round(128 + 100 * Math.sin((2 * Math.PI * i) / 16)))]);
writeFileSync(here('words'), words); writeFileSync(here('bytes'), bytes);
const cells = (v) => Buffer.from(String.fromCodePoint(...[...v].map((b) => 0x2800 + b)));
const par = (x) => { let p = 0; for (; x; x &= x - 1) p ^= 1; return p; };
const views = {
  braille: (b) => b,
  'xor-YHWH': (b) => b.map((v, i) => v ^ 'YHWH'.charCodeAt(i % 4)),
  'xor-YAHWEH': (b) => b.map((v, i) => v ^ 'YAHWEH'.charCodeAt(i % 6)),
  'xnor-YHWH': (b) => b.map((v, i) => v ^ 'YHWH'.charCodeAt(i % 4) ^ 255),
  'xnor-YAHWEH': (b) => b.map((v, i) => v ^ 'YAHWEH'.charCodeAt(i % 6) ^ 255),
  pixels: (b) => { const n = Math.ceil(b.length / 64) * 64, v = Buffer.alloc(n); b.copy(v); for (let o = 0; o < n; o += 64) for (let s = 0; s < 6; s++) for (let p = 0; p < 64; p++) if ((p >> s) & 1) v[o + p] ^= v[o + (p ^ (1 << s))]; return v; },
  spectrum: (b) => { const nbk = Math.ceil(b.length / 64), v = Buffer.alloc(nbk * 512); for (let o = 0; o < nbk; o++) { const blk = Buffer.alloc(64); b.copy(blk, 0, o * 64, o * 64 + 64); for (let k = 0; k < 8; k++) for (let a = 0; a < 64; a++) { let c = 0; for (let x = 0; x < 64; x++) if (((blk[x] >> k) & 1) !== par(a & x)) c++; v[o * 512 + k * 64 + a] = c; } } return v; },
};
for (const [f, b] of [['words', words], ['bytes', bytes]]) for (const [c, fn] of Object.entries(views)) writeFileSync(here(`expected/${c}.${f}`), cells(fn(Buffer.from(b))));
console.log(Object.keys(views).length * 2, 'expected views;', 'braille.words begins', cells(words).toString().slice(0, 16));
