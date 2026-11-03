// fetch.mjs - on the host, once, with the author's yes: each case of cases.mjs expanded by Wikipedia's own parser,
// its expandtemplates API, the template's body given by TemplateSandbox for that request only (nothing created or
// edited on Wikipedia); each expansion kept as a literal, cases/NN.body, NN.call, NN.expanded. Run once; the check
// never fetches.
//	node fetch.mjs <out: in/wiki/cases>
import { mkdirSync, writeFileSync } from "fs";
import { cases } from "./cases.mjs";
const [OUT] = process.argv.slice(2);
mkdirSync(OUT, { recursive: true });
const UA = "unbound-template-check/1.0 (https://github.com/bthornemail/unbound)";
const nn = (i) => String(i + 1).padStart(2, "0");
for (const [i, [body, call]] of cases.entries()) {
  const form = new URLSearchParams({ action: "expandtemplates", format: "json", formatversion: "2", prop: "wikitext", title: "Sandbox", text: call,
    templatesandboxtitle: "Template:UnboundBindTest", templatesandboxtext: body });
  const r = await fetch("https://en.wikipedia.org/w/api.php", { method: "POST", headers: { "User-Agent": UA, "Content-Type": "application/x-www-form-urlencoded" }, body: form });
  const j = await r.json();
  if (!j.expandtemplates) throw new Error(`${nn(i)}: ${JSON.stringify(j).slice(0, 300)}`);
  writeFileSync(`${OUT}/${nn(i)}.body`, body);
  writeFileSync(`${OUT}/${nn(i)}.call`, call);
  writeFileSync(`${OUT}/${nn(i)}.expanded`, j.expandtemplates.wikitext);
  await new Promise((ok) => setTimeout(ok, 1000));
}
console.log(cases.length, "cases expanded by Wikipedia, kept in", OUT);
