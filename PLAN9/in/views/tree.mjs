// tree.mjs - on the host: the goldens of zviews, each page's outline by omi-types' OMI.View (Outline.hs, built
// with ghc), and the falsifier: each page cut by its first end tag, as pieces.mjs reads it, and the cut page's
// outline by OMI.View. A cut page's outline must differ from its page's; zviews must give OMI.View's for both.
//	node tree.mjs <unbound/HTML> <out: in/views/tree> <outline>
import { readdirSync, readFileSync, writeFileSync, mkdirSync } from "fs";
import { execFileSync } from "child_process";
import { pieces } from "./pieces.mjs";
const [HTML, OUT, OUTLINE] = process.argv.slice(2);
const files = readdirSync(HTML).filter((f) => f.endsWith(".html")).sort();
mkdirSync(OUT, { recursive: true });
const outline = (f) => execFileSync(OUTLINE, [f], { maxBuffer: 1 << 26 });
const opens = (o) => o.toString("latin1").split("\n").filter((l) => l.trim() === "OPEN").length;
const nn = (i) => String(i + 1).padStart(2, "0");
let before = 0, after = 0;
files.forEach((f, i) => {
  const text = readFileSync(`${HTML}/${f}`, "utf8"), { runes, pieces: ps } = pieces(text);
  const e = ps.find((p) => p.prod === "ENDTAG");
  const cut = runes.slice(0, e.a).join("") + runes.slice(e.b).join("");
  writeFileSync(`${OUT}/${nn(i)}.cut`, cut);
  const o = outline(`${HTML}/${f}`), c = outline(`${OUT}/${nn(i)}.cut`);
  if (o.equals(c)) throw new Error(`${f}: cut by its first end tag, its outline unchanged`);
  writeFileSync(`${OUT}/${nn(i)}.outline`, o);
  writeFileSync(`${OUT}/${nn(i)}.cut.outline`, c);
  before += opens(o); after += opens(c);
});
writeFileSync(`${OUT}/pages`, files.map((f) => f + "\n").join(""));
console.log(files.length, "outlines;", files.length, "cut pages, each outline changed; elements left open", before, "in the pages,", after, "in the cut pages");
