// pieces.mjs - the pieces of a page, by wsn/html.rules: each piece its production, its start and end (in runes)
// and its cast (wsn/html.wsn's alphabet). The reader is deterministic and reads each rune once.
const RAW = new Set(["script", "style", "title", "textarea"]);
const KEEP = new Set([..."<>/=\"'-_:.#&;!"]);
export function cast(ch) {
  if (/^[A-Za-z]$/.test(ch)) return ch.toLowerCase();
  if (/^[0-9]$/.test(ch) || KEEP.has(ch)) return ch;
  if (ch === " " || ch === "\t" || ch === "\n" || ch === "\r" || ch === "\f") return " ";
  return "~";
}
const SP = (c) => c === " " || c === "\t" || c === "\n" || c === "\r" || c === "\f";
const LET = (c) => /^[A-Za-z]$/.test(c);
export function pieces(text) {
  const r = Array.from(text), n = r.length, out = [];
  const lower = (i, s) => r.slice(i, i + s.length).join("").toLowerCase() === s;
  let i = 0;
  const push = (prod, a, b, extra = {}) => { out.push({ prod, a, b, ...extra }); };
  const tagEnd = (j) => {	// j after the name: attributes, quoted values skipped, to the closing ">"
    let q = null;
    for (; j < n; j++) {	// a quote opens a value when the nearest rune before it, spaces aside, is "="
      const c = r[j];
      if (q) { if (c === q) q = null; }
      else if (c === '"' || c === "'") { let k = j - 1; while (SP(r[k])) k--; if (r[k] === "=") q = c; }
      else if (c === ">") return j + 1;
    }
    return -1;
  };
  while (i < n) {
    if (r[i] === "<") {
      if (lower(i, "<!--")) { let e = i + 4; while (e < n && !(r[e] === "-" && r[e + 1] === "-" && r[e + 2] === ">")) e++; const b = e < n ? e + 3 : n; push(e < n ? "COMMENT" : "TEXT", i, b); i = b; continue; }
      if (lower(i, "<!doctype")) { let e = i; while (e < n && r[e] !== ">") e++; push("DOCTYPE", i, e + 1); i = e + 1; continue; }
      const close = r[i + 1] === "/";
      const s = i + (close ? 2 : 1);
      if (LET(r[s])) {
        let j = s; while (j < n && /[A-Za-z0-9\-_:.]/.test(r[j])) j++;
        const name = r.slice(s, j).join("").toLowerCase();
        const e = tagEnd(j);
        if (e > 0) {
          const self = !close && r[e - 2] === "/";
          push(close ? "ENDTAG" : "STARTTAG", i, e, { name, self });
          i = e;
          if (!close && !self && RAW.has(name)) {	// raw text to "</name", in any case, or the end
            let k = i; while (k < n && !(r[k] === "<" && r[k + 1] === "/" && lower(k + 2, name))) k++;
            if (k > i) push("RAWTEXT", i, k);
            i = k;
          }
          continue;
        }
      }
    }
    let e = i + 1;	// text to the next "<" that begins a piece
    for (; e < n; e++) if (r[e] === "<" && (r[e + 1] === "!" || r[e + 1] === "/" || LET(r[e + 1]))) break;
    if (out.length && out[out.length - 1].prod === "TEXT" && out[out.length - 1].b === i) out[out.length - 1].b = e; else push("TEXT", i, e);
    i = e;
  }
  for (const p of out) p.cast = r.slice(p.a, p.b).map(cast).join("");
  return { runes: r, pieces: out };
}
// A start tag's attributes, by html.wsn: STARTTAG = "<" NAME { SPACES ATTR } [ SPACES ] [ "/" ] ">", ATTR = ATTRNAME
// [ [ SPACES ] "=" [ SPACES ] VALUE ], VALUE a quoted run (its quotes in it) or UQ { UQ }, read over the cast: each
// attribute an ATTR span, its ATTRNAME, its VALUE when it has one, at the page's rune offsets.
const AN = (c) => c !== undefined && !" =\"'/>".includes(c);
const UQ = (c) => c !== undefined && !" =\"'<>".includes(c);
export function attrs(runes, p) {
  const c = runes.slice(p.a, p.b).map(cast), out = [];
  let j = 1 + p.name.length;
  for (;;) {
    const s0 = j;
    while (c[j] === " ") j++;
    if (c[j] === ">" || (c[j] === "/" && c[j + 1] === ">")) break;
    if (j === s0 || !AN(c[j])) throw new Error(`not an attribute at ${p.a + j}`);
    const a = j;
    while (AN(c[j])) j++;
    const ne = j;
    let k = j, v = null;
    while (c[k] === " ") k++;
    if (c[k] === "=") {
      k++;
      while (c[k] === " ") k++;
      const vs = k;
      if (c[k] === '"' || c[k] === "'") { const q = c[k++]; while (c[k] !== q) k++; k++; } else while (UQ(c[k])) k++;
      if (k === vs) throw new Error(`no value at ${p.a + k}`);
      v = [vs, k];
      j = k;
    }
    out.push({ prod: "ATTR", a: p.a + a, b: p.a + (v ? v[1] : ne) }, { prod: "ATTRNAME", a: p.a + a, b: p.a + ne });
    if (v) out.push({ prod: "VALUE", a: p.a + v[0], b: p.a + v[1] });
  }
  for (const s of out) s.cast = runes.slice(s.a, s.b).map(cast).join("");
  return out;
}
