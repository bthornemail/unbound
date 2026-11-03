// match.mjs - on the host: templates over the bound pieces, literal regular expressions (regex as it is: Plan 9's
// regexp(2) in the guest, JavaScript's RegExp here, the two to agree), and what each names on each page. The spans
// are the pieces (pieces.mjs) and each start tag's ATTR, ATTRNAME and VALUE (attrs), in page order, a tag before its
// attributes. A template is a line, its production, a tab, its pattern over the cast; it names each span of its
// production whose cast the pattern matches. The templates are taken from the pages, not written: each element's
// start tag, ^<name[ />]; each attribute name, ^name$; each value found on two pages or more, ^value$ ("." the one
// cast symbol regex reads otherwise, written \.). A match is a line: the template's ordinal, the production, the
// span's first and past-last rune offsets, its runes (a line feed \n, a tab \t, a backslash \\). The falsifier: two
// mutants of each template, its last symbol the next in the alphabet, its first symbol "." (any); their matches
// filed as they come, fewer, more or the same.
//	node match.mjs <unbound/HTML> <out: in/views/match>
import { readdirSync, readFileSync, writeFileSync, mkdirSync } from "fs";
import { pieces, attrs, cast } from "./pieces.mjs";
const [HTML, OUT] = process.argv.slice(2);
const ALPHA = [..."abcdefghijklmnopqrstuvwxyz0123456789<>/=\"'-_:.#&;! ~"];
const files = readdirSync(HTML).filter((f) => f.endsWith(".html")).sort();
mkdirSync(OUT, { recursive: true });
const pages = files.map((f) => {
  const { runes, pieces: ps } = pieces(readFileSync(`${HTML}/${f}`, "utf8")), spans = [];
  for (const p of ps) { spans.push(p); if (p.prod === "STARTTAG") spans.push(...attrs(runes, p)); }
  return { f, runes, spans };
});
// the templates, from the pages
const els = new Set(), names = new Set(), vals = new Map();
for (const { f, spans } of pages) for (const s of spans) {
  if (s.prod === "STARTTAG") els.add(s.name);
  if (s.prod === "ATTRNAME") names.add(s.cast);
  if (s.prod === "VALUE") (vals.get(s.cast) ?? vals.set(s.cast, new Set()).get(s.cast)).add(f);
}
const sorted = (xs) => [...xs].sort();
const T = [
  ...sorted(els).map((n) => ["STARTTAG", "^<", [...n], "[ />]"]),
  ...sorted(names).map((n) => ["ATTRNAME", "^", [...n], "$"]),
  ...sorted([...vals].filter(([, s]) => s.size > 1).map(([v]) => v)).map((v) => ["VALUE", "^", [...v], "$"]),
];
const sym = (c) => (c === "." ? "\\." : c);
const line = ([prod, pre, body, post]) => `${prod}\t${pre}${body.map(sym).join("")}${post}`;
const next = (c) => ALPHA[(ALPHA.indexOf(c) + 1) % ALPHA.length];
const M = T.flatMap(([prod, pre, body, post]) => [
  [prod, pre, [...body.slice(0, -1), next(body.at(-1))], post],
  [prod, pre, [".", ...body.slice(1)].map((c, i) => (i === 0 ? null : c)), post],
]);
const mline = (t) => (t[2][0] === null ? `${t[0]}\t${t[1]}.${t[2].slice(1).map(sym).join("")}${t[3]}` : line(t));
const tl = T.map(line), ml = M.map(mline);
writeFileSync(`${OUT}/templates`, tl.map((l) => l + "\n").join(""));
writeFileSync(`${OUT}/mutants`, ml.map((l) => l + "\n").join(""));
const esc = (s) => s.replace(/\\/g, "\\\\").replace(/\n/g, "\\n").replace(/\t/g, "\\t");
const run = (ls, page) => {
  const out = [], counts = [];
  ls.forEach((l, i) => {
    const [prod, pat] = l.split("\t"), re = new RegExp(pat);
    let n = 0;
    for (const s of page.spans) if (s.prod === prod && re.test(s.cast)) {
      if (page.runes.slice(s.a, s.b).map(cast).join("") !== s.cast) throw new Error("a span's runes not its cast");
      out.push(`${i + 1}\t${prod}\t${s.a}\t${s.b}\t${esc(page.runes.slice(s.a, s.b).join(""))}\n`); n++;
    }
    counts.push(n);
  });
  return { text: out.join(""), counts };
};
const nn = (i) => String(i + 1).padStart(2, "0");
const tc = new Array(T.length).fill(0), mc = new Array(M.length).fill(0);
pages.forEach((p, i) => {
  const a = run(tl, p), b = run(ml, p);
  writeFileSync(`${OUT}/${nn(i)}`, a.text);
  writeFileSync(`${OUT}/${nn(i)}.mut`, b.text);
  a.counts.forEach((c, k) => (tc[k] += c));
  b.counts.forEach((c, k) => (mc[k] += c));
});
writeFileSync(`${OUT}/pages`, files.map((f) => f + "\n").join(""));
if (tc.some((c) => c === 0)) throw new Error("a template from the pages names nothing");
let fewer = 0, more = 0, same = 0;
mc.forEach((c, k) => { const t = tc[k >> 1]; if (c < t) fewer++; else if (c > t) more++; else same++; });
const total = tc.reduce((a, b) => a + b, 0);
console.log(files.length, "pages;", T.length, "templates (", els.size, "elements,", names.size, "attribute names,", T.length - els.size - names.size, "values on two pages or more ) naming", total, "spans;", M.length, "mutants:", fewer, "fewer,", more, "more,", same, "the same");
