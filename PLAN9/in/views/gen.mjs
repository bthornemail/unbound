// gen.mjs - on the host: the questions views.rc asks of wsn/html.wsn by zlazy. Every page of unbound/HTML is read
// into pieces by pieces.mjs (wsn/html.rules), and checked to give back the page byte for byte; each distinct
// piece but raw text is a question, its production and its cast (prods and casts, a line each, in step); raw text
// is checked by its rule here (it holds no end of its own name). The falsifier asks each piece of the wrong
// production: a tag, comment or doctype as TEXT, a text run as STARTTAG (wrong.prods); every answer must be no.
//	node gen.mjs <unbound/HTML> <out>
import { readdirSync, readFileSync, writeFileSync } from "fs";
import { pieces } from "./pieces.mjs";
const [HTML, OUT] = process.argv.slice(2);
const files = readdirSync(HTML).filter((f) => f.endsWith(".html")).sort();
const q = new Map();
let n = 0, raw = 0;
for (const f of files) {
  const text = readFileSync(`${HTML}/${f}`, "utf8"), { runes, pieces: ps } = pieces(text);
  if (ps.map((p) => runes.slice(p.a, p.b).join("")).join("") !== text) throw new Error(`${f}: not its pieces`);
  ps.forEach((p, i) => {
    n++;
    if (p.prod === "RAWTEXT") { raw++; if (p.cast.includes(`</${ps[i - 1].name}`)) throw new Error(`${f}: raw text holds its end`); return; }
    q.set(`${p.prod}\t${p.cast}`, p.prod);
  });
}
const keys = [...q.keys()].sort();
writeFileSync(`${OUT}/prods`, keys.map((k) => k.split("\t")[0] + "\n").join(""));
writeFileSync(`${OUT}/casts`, keys.map((k) => k.slice(k.indexOf("\t") + 1) + "\n").join(""));
writeFileSync(`${OUT}/wrong.prods`, keys.map((k) => (k.startsWith("TEXT\t") ? "STARTTAG" : "TEXT") + "\n").join(""));
console.log(files.length, "pages give back their bytes from", n, "pieces;", raw, "raw texts hold no end of their own name;", keys.length, "distinct questions");
