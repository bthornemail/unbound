// gen.mjs - on the host: the goldens for runes.rc, the check of Plan 9's rune routines (libc's chartorune,
// runetochar, runelen, fullrune, utfnlen; 9legacy: Rune 32 bits, Runemax 0x10FFFF, UTFmax 4) against UTF-8 read
// another way. Written: all.utf8, node's TextEncoder of every Unicode scalar value in order; node, TextDecoder's
// (WHATWG) reading of 143,768 byte strings, each followed by "A"; plan9, the prediction of chartorune's reading of
// the same strings, from its source in the guest (/sys/src/libc/port/rune.c, read 2026-11-02 before the guest ran
// anything), filed before the check; odd and fullrune, the predictions of runetochar and runelen on surrogates
// and runes past Runemax, and of fullrune on every first byte and length 0 to 4. The strings, in order: every
// byte; every two bytes; a lead 0xC0 to 0xFF then two of S; a lead 0xF0 to 0xF7 then three of S, S the bytes
// 00 41 7F 80 8F 90 9F A0 BF C0 C2 DF E0 ED EF F0 F4 F5 FF (zrunes.c makes the same list). Each line the
// reading's code points in hex.
//	node gen.mjs <out>
import { writeFileSync } from "fs";
const OUT = process.argv[2];
const S = [0x00, 0x41, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbf, 0xc0, 0xc2, 0xdf, 0xe0, 0xed, 0xef, 0xf0, 0xf4, 0xf5, 0xff];
const strings = [];
for (let a = 0; a < 256; a++) strings.push([a]);
for (let a = 0; a < 256; a++) for (let b = 0; b < 256; b++) strings.push([a, b]);
for (let a = 0xc0; a <= 0xff; a++) for (const b of S) for (const c of S) strings.push([a, b, c]);
for (let a = 0xf0; a <= 0xf7; a++) for (const b of S) for (const c of S) for (const d of S) strings.push([a, b, c, d]);
const hex = (cps) => cps.map((c) => c.toString(16)).join(" ") + "\n";

// every scalar value, as node writes it
const parts = [];
for (let c = 0; c <= 0x10ffff; c++) if (c < 0xd800 || c > 0xdfff) parts.push(String.fromCodePoint(c));
writeFileSync(`${OUT}/all.utf8`, Buffer.from(parts.join(""), "utf8"));

// node's reading
const dec = new TextDecoder("utf-8");
writeFileSync(`${OUT}/node`, strings.map((s) => hex(Array.from(dec.decode(Uint8Array.from([...s, 0x41]))).map((ch) => ch.codePointAt(0)))).join(""));

// chartorune, as its source reads (Tx 0x80, Testx 0xC0, T2 0xC0, T3 0xE0, T4 0xF0, T5 0xF8, Bitx 6): a byte under
// 0x80 is itself; otherwise up to three more bytes, each a continuation (10xxxxxx) or bad; at the first i where
// the lead is under T(i+2) the value, masked, must be past the shorter forms (two bytes: lead at least 0xC0 and
// past 0x7F; three: past 0x7FF, not a surrogate; four: past 0xFFFF, not past 0x10FFFF); a lead from 0xF8 bad.
// Bad is Runeerror (0xFFFD), one byte consumed. The string is followed by "A" and a NUL, which no lead reads past.
function chartorune(b, i) {
  const c0 = b[i];
  if (c0 < 0x80) return [c0, 1];
  let l = c0;
  for (let k = 1; k < 4; k++) {
    const ck = (b[i + k] ?? 0) ^ 0x80;
    if (ck & 0xc0) return [0xfffd, 1];
    l = (l << 6) | ck;
    const T = [0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8][k + 2];
    if (c0 < T) {
      if (k === 1) { l &= 0x7ff; if (c0 < 0xc0 || l <= 0x7f) return [0xfffd, 1]; }
      else if (k === 2) { l &= 0xffff; if (l <= 0x7ff || (l >= 0xd800 && l <= 0xdfff)) return [0xfffd, 1]; }
      else { l &= 0x1fffff; if (l <= 0xffff || l > 0x10ffff) return [0xfffd, 1]; }
      return [l, k + 1];
    }
  }
  return [0xfffd, 1];
}
const plan9 = (s) => { const b = [...s, 0x41, 0]; const out = []; for (let i = 0; i < s.length + 1;) { const [r, n] = chartorune(b, i); out.push(r); i += n; } return out; };
writeFileSync(`${OUT}/plan9`, strings.map((s) => hex(plan9(s))).join(""));

// runetochar and runelen on what is not a scalar value: Runeerror's three bytes; and fullrune
const odd = [];
for (let c = 0xd800; c <= 0xdfff; c++) odd.push(c);
odd.push(0x110000, 0x110001, 0x1fffff, 0x200000, 0x7fffffff, 0xffffffff);
writeFileSync(`${OUT}/odd`, odd.map((c) => `${c.toString(16)} 3 efbfbd\n`).join(""));
const full = [];
for (let c = 0; c < 256; c++) for (let n = 0; n <= 4; n++) {
  const need = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
  full.push(`${c.toString(16)} ${n} ${n > 0 && n >= need ? 1 : 0}\n`);
}
writeFileSync(`${OUT}/fullrune`, full.join(""));
let agree = 0;
const nd = strings.map((s) => hex(Array.from(dec.decode(Uint8Array.from([...s, 0x41]))).map((ch) => ch.codePointAt(0))));
strings.forEach((s, i) => { if (nd[i] === hex(plan9(s))) agree++; });
console.log(strings.length, "strings;", parts.length, "scalar values; predicted: Plan 9 and node agree on", agree, "and differ on", strings.length - agree);
