// gen.mjs - on the host: the goldens of ztranslate, the second Unicode address (the author: "that is just a decoder
// example, like a translator for all bytes", "all it does its like unicode for all unicode"), read from
// unbound/HTML/slide_ruler.html, the 16-Bit Kernel Slide Ruler: a code point is plane << 16 | offset, the plane shown
// by the Aegean sign AEGEAN_START + plane and the offset by the Braille cell BRAILLE_START + a byte; the constants
// taken from the page itself. The page shows the offset's low byte alone (res % 256); the address here gives both
// bytes, so every code point has one: an Aegean sign, the high byte's cell, the low byte's cell.
//	table.gz	a line for each code point 0 to 10FFFF in order (line n + 1 the code point n): its address;
//			gzipped, 12,255,232 bytes as 1,114,112 lines (gunzip in Plan 9 gives them back)
//	wrong		lines that are no address: each must be refused
//	counts.*	what ztranslate -d reads back from the table, from wrong, and from the falsifier (ztranslate -n,
//			the page's one cell as the address, the high byte it does not show read as the blank cell)
//	node gen.mjs <unbound/HTML/slide_ruler.html> <out>
import { readFileSync, writeFileSync } from "fs";
import { gzipSync } from "zlib";
const [PAGE, OUT] = process.argv.slice(2);
const page = readFileSync(PAGE, "utf8");
const AEGEAN = Number(/const AEGEAN_START = (0x[0-9A-Fa-f]+);/.exec(page)[1]), BRAILLE = Number(/const BRAILLE_START = (0x[0-9A-Fa-f]+);/.exec(page)[1]);
const PLANES = Number(/const planeIdx = x % (\d+);/.exec(page)[1]);
const ch = String.fromCodePoint;
const addr = (n) => ch(AEGEAN + (n >> 16)) + ch(BRAILLE + ((n >> 8) & 255)) + ch(BRAILLE + (n & 255));
const out = [];
for (let n = 0; n < PLANES << 16; n++) out.push(addr(n) + "\n");
writeFileSync(`${OUT}/table.gz`, gzipSync(Buffer.from(out.join(""), "utf8"), { level: 9 }));
// the page's display, one cell, as an address: the high byte it does not show read as the blank cell
let same = 0;
for (let n = 0; n < PLANES << 16; n++) if ((((n >> 16) << 16) | (n & 255)) === n) same++;
const wrong = [
  ch(AEGEAN + PLANES) + ch(BRAILLE) + ch(BRAILLE),       // past the 17th plane
  ch(AEGEAN - 1) + ch(BRAILLE) + ch(BRAILLE),            // before the first sign
  ch(AEGEAN) + ch(BRAILLE + 256) + ch(BRAILLE),           // past the Braille block
  ch(AEGEAN) + ch(BRAILLE - 1) + ch(BRAILLE),
  ch(AEGEAN) + ch(BRAILLE),                               // two glyphs: the page's display
  ch(AEGEAN) + ch(BRAILLE) + ch(BRAILLE) + ch(BRAILLE),   // four
  ch(BRAILLE) + ch(AEGEAN) + ch(BRAILLE),                 // out of place
  "A" + ch(BRAILLE) + ch(BRAILLE), "", "⠀⠀⠀",
];
writeFileSync(`${OUT}/wrong`, wrong.map((l) => l + "\n").join(""));
const total = PLANES << 16;
const line = (n, own, other, refused) => `${n} lines, ${own} at their own place, ${other} elsewhere, ${refused} refused\n`;
writeFileSync(`${OUT}/counts.table`, line(total, total, 0, 0));
writeFileSync(`${OUT}/counts.wrong`, line(wrong.length, 0, 0, wrong.length));
writeFileSync(`${OUT}/counts.page`, line(total, same, total - same, 0));
console.log("AEGEAN_START", AEGEAN.toString(16), "BRAILLE_START", BRAILLE.toString(16), "planes", PLANES, "; table", out.length, "lines; the page's one cell reads back", same, "of", total, "; first", [...addr(0)].map((c) => c.codePointAt(0).toString(16)).join(" "), "last", [...addr(total - 1)].map((c) => c.codePointAt(0).toString(16)).join(" "));
