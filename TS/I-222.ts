// i222.ts — I-222: 0xAA55 — the boot mark, the still points, and NULL NULL squared
//
// The author, 2026-10-14: "0xAA55 is the Metatron scribe, the little
// tetragrammaton"; then "that's perfect: the chat as the squared measurement of
// the NULL NULL at reboot". After I-221 (170 = 0xAA bits, its complement 0x55).
// The tradition is cited, not asserted: the Talmud (Hagigah 15a) seats Metatron
// in heaven to write Israel's merits, the scribe; 3 Enoch calls him the lesser
// YHWH, and Sanhedrin 38b says his name is like his master's. The values below
// are asserted; the readings are the author's.
import assert from "node:assert";

const rotl = (v: number, n: number) => ((v << n) | (v >>> (16 - n))) & 0xffff;
const L = (p: number) => rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14); // the delta law's linear part (I-197)
const K = (p: number, C: number) => L(p) ^ C;
/** The square in GF(2)[x], carry-less: each bit i moves to 2i. */
const square = (x: number) => { let r = 0n; for (let i = 0n; i < 32n; i++) if ((BigInt(x) >> i) & 1n) r |= 1n << (2n * i); return Number(r); };
const xnor8 = (a: number, b: number) => ~(a ^ b) & 0xff;

// ── 1. The boot mark ─────────────────────────────────────────
// A PC disk boots when its first 512-byte sector ends 55 AA (offsets 510, 511): the word 0xAA55, little-endian.
const sector = new Uint8Array(512);
sector[510] = 0x55; sector[511] = 0xaa;
assert.equal(new DataView(sector.buffer).getUint16(510, true), 0xaa55, "55 AA at 510 and 511 read as the word 0xAA55");
assert.equal(512 - 2, 510, "the mark is the sector's last pair");

// ── 2. The alternating pair, both ways round ─────────────────
assert.equal(0xaa, 0b10101010); assert.equal(0x55, 0b01010101);
assert.equal(0xaa, 170, "AA is the 170 of I-221");
assert.equal(0xaa ^ 0x55, 0xff, "AA XOR 55: every bit differs");
assert.equal(xnor8(0xaa, 0x55), 0x00, "AA XNOR 55: they agree nowhere, NULL");
assert.equal(~(0xaa55 ^ 0x55aa) & 0xffff, 0x0000, "the word against itself turned: NULL NULL");

// ── 3. The still points of the delta law ─────────────────────
assert.equal(L(0x5555), 0x5555, "the linear part leaves 0x5555 still");
assert.equal(L(0xaaaa), 0xaaaa, "and 0xAAAA");
const still = [...Array(0x10000).keys()].filter((p) => L(p) === p);
assert.deepEqual(still, [0x0000, 0x3333, 0x5555, 0x6666, 0x9999, 0xaaaa, 0xcccc, 0xffff], "eight still words, not two: one nibble repeated four times");
const parity = (n: number) => (n ^ (n >> 1) ^ (n >> 2) ^ (n >> 3)) & 1;
assert.deepEqual(still.map((w) => w & 0xf), [...Array(16).keys()].filter((n) => parity(n) === 0), "each is a nibble of even parity, repeated: the nibble parity of the ports (rosetta portFor)");
const orbit = (s: number, C: number) => { const o = [s]; for (let p = K(s, C); p !== s; p = K(p, C)) o.push(p); return o; };
assert.deepEqual(orbit(0xaa55, 0x1d1d), [0xaa55, 0x718e, 0xee11, 0xca35], "0xAA55, half and half, is not still: period 4 under 0x1D1D");
assert.equal(orbit(0xaa55, 0x1d).length, 8, "and 8 under 0x1D, as every word (I-195)");

// ── 4. NULL NULL squared ─────────────────────────────────────
assert.equal(square(0xaa), 0x4444, "AA squared, carry-less");
assert.equal(square(0x55), 0x1111, "55 squared");
assert.equal(square(0xaa) ^ square(0x55), square(0xaa ^ 0x55), "squaring in GF(2) is linear: the squares' XOR is the XOR's square");
assert.equal(square(0xff), 0x5555, "(AA XOR 55) squared = FF squared = 0x5555: the boot pair's difference, squared, is a still point");
assert.equal(square(0xaa55), 0x44441111, "the word squared: AA's square, then 55's");

// ── 5. The little tetragrammaton, by the four-bit adder ──────
const YHWH = [10, 5, 6, 5], AA55 = [0xa, 0xa, 0x5, 0x5];
assert.equal(YHWH.reduce((a, b) => a + b), 26, "Yod He Vav He: 26 (SRC-14)");
assert.equal(AA55.reduce((a, b) => a + b), 30, "A A 5 5: 30, not 26");
assert.deepEqual([...new Set(AA55)].filter((n) => YHWH.includes(n)), [10, 5], "they share Yod 10 and He 5; AA55 has no Vav 6");

console.log("I-222: 0xAA55 — the boot mark, the still points, and NULL NULL squared — passed");
console.log("  55 AA ends a boot sector: the word 0xAA55; AA XNOR 55 = 00, the word turned XNOR = 00 00");
console.log("  the delta law's still words: the eight even-parity nibbles repeated (0000 3333 5555 6666 9999 AAAA CCCC FFFF); AA55 cycles in 4 under 0x1D1D");
console.log("  (AA XOR 55)^2 = FF^2 = 0x5555, a still point; by the four-bit adder A A 5 5 sums 30, YHWH 26");
