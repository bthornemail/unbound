// gen.mjs - on the host: the goldens of ztranslit, ZIGUAROTH's _ARCHIVE/Expressions/transliterate.js (its rules T1 to
// T6, _ARCHIVE/Configurations/CONFIGURATION.yaml: a transliteration changes neither the template's file nor its
// ASCII, U+0000 to U+007F, its syntax; it changes only the code points its mapping names; a mapping is a static file
// of quoted code-point pairs, each mapped once; the output is checked against its template, the same length in code
// points and every change a mapped one), ported to unbound with the author's yes. The first mapping, the author's:
// Braille dark and light, each cell U+2800 + b to U+2800 + (b XOR 0xFF), its complement (domino-alpha, global.vtt).
// The templates: the files of unbound holding Braille, and ZIGUAROTH's _TEMPLATES/term.html (in unbound already).
// Each run is transliterate.js's own, from in/translit, its output and its message filed; the mappings it refuses,
// its messages. The falsifier, ztranslit -n, the ASCII guard dropped: a mapping naming "A" passes the load, and the
// check after (T5, ASCII untouched) must refuse at the first "A"; what that gives is computed here by the same steps.
//	node gen.mjs <ZIGUAROTH> <unbound> <out: in/translit>
import { readFileSync, writeFileSync, mkdirSync, copyFileSync, rmSync } from "fs";
import { spawnSync } from "child_process";
const [ZIG, UNB, OUT] = process.argv.slice(2);
const T = `${ZIG}/_ARCHIVE/Expressions/transliterate.js`;
mkdirSync(`${OUT}/templates`, { recursive: true });
mkdirSync(`${OUT}/mappings`, { recursive: true });
mkdirSync(`${OUT}/out`, { recursive: true });
const sources = [`${ZIG}/_TEMPLATES/term.html`, `${UNB}/HTML/slide_ruler.html`, `${UNB}/PLAN9/wsn/unbound/HTML/slide_ruler.html.wsn`, `${UNB}/PLAN9/wsn/js/catalog/I-196.wsn`,
  ...["agi", "canticle", "core-octad", "covenant", "epilogue", "logos", "manifest", "prelude", "revelation", "speaker-for-the-untitled", "tetragrammatron"].map((n) => `${UNB}/VTT/NARRATIVE/${n}.vtt`)];
const names = sources.map((s) => s.split("/").pop());
sources.forEach((s, i) => copyFileSync(s, `${OUT}/templates/${names[i]}`));
const hex = (n) => n.toString(16).toUpperCase().padStart(4, "0");
const pair = (a, b) => `"U+${hex(a)}": "U+${hex(b)}"\n`;
const maps = {
  "braille-complement.yaml": "# Braille dark and light: each cell to its complement, U+2800 + b to U+2800 + (b XOR 0xFF)\n" + [...Array(256).keys()].map((b) => pair(0x2800 + b, 0x2800 + (b ^ 0xff))).join(""),
  "ascii.yaml": pair(0x41, 0x391),
  "ascii-target.yaml": pair(0x2800, 0x41),
  "twice.yaml": pair(0x2800, 0x28ff) + pair(0x2801, 0x28fe) + pair(0x2800, 0x28fd),
  "form.yaml": pair(0x2800, 0x28ff) + '"U+2801" -> "U+28FE"\n',
  "lower.yaml": '"U+28ff": "U+2800"\n',
  "surrogate.yaml": pair(0x2800, 0xd800),
  "past.yaml": '"U+2800": "U+110000"\n',
  "short.yaml": '"U+280": "U+28FF"\n',
};
for (const [n, t] of Object.entries(maps)) writeFileSync(`${OUT}/mappings/${n}`, t);
const run = (args) => spawnSync("node", [T, ...args], { cwd: OUT, encoding: "utf8" });
const msgs = [];
for (const n of names) {
  const r = run([`templates/${n}`, "mappings/braille-complement.yaml", `out/${n}`]);
  if (r.status !== 0) throw new Error(`${n}: ${r.stderr}`);
  msgs.push(r.stderr.trim());
  const back = run([`out/${n}`, "mappings/braille-complement.yaml", `out/${n}.twice`]);
  if (back.status !== 0 || !readFileSync(`${OUT}/out/${n}.twice`).equals(readFileSync(`${OUT}/templates/${n}`))) throw new Error(`${n}: twice is not the template`);
  rmSync(`${OUT}/out/${n}.twice`);
}
writeFileSync(`${OUT}/messages`, msgs.map((m) => m + "\n").join(""));
const refusals = [];
for (const n of Object.keys(maps).filter((n) => n !== "braille-complement.yaml")) {
  const r = run(["templates/term.html", `mappings/${n}`]);
  if (r.status === 0) throw new Error(`${n}: not refused`);
  refusals.push(`${n}: ${/^Error: (.*)$/m.exec(r.stderr)[1]}`);
}
writeFileSync(`${OUT}/refusals`, refusals.map((m) => m + "\n").join(""));
// the falsifier: transliterate.js's steps with the ASCII guard dropped, the mapping ascii.yaml
const falsified = names.map((n) => {
  const src = [...readFileSync(`${OUT}/templates/${n}`, "utf8")], i = src.findIndex((c) => c === "A");
  return i < 0 ? `templates/${n}: ${src.length} code points, 0 mapped (1 in the mapping), ASCII untouched` : `position ${i}: an unmapped change`;
});
writeFileSync(`${OUT}/falsified`, falsified.map((m) => m + "\n").join(""));
writeFileSync(`${OUT}/templates.list`, names.map((n) => n + "\n").join(""));
console.log(names.length, "templates, each mapped and mapped again to itself;", refusals.length, "mappings refused;", falsified.filter((f) => f.startsWith("position")).length, "templates where -n is refused at its first A");
console.log(msgs.join("\n"));
