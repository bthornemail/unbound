// gen.mjs - the cases for src/repfs.rc and their expected answers, computed here in JavaScript (BigInt), apart from
// repfs.c: for each path, whether it resolves; if it does, its qid path (num<<32 | den, 16 hex digits as ls -q prints
// it) and its listing, the reduced fraction N÷D and the decimal I.P(R), sorted as ls sorts (bytes).
//	node gen.mjs	writes cases and expected
import { writeFileSync } from 'node:fs';
const gcd = (a, b) => (b ? gcd(b, a % b) : a);
const ok = (n, d) => { const g = gcd(n, d) || 1n; n /= g; d /= g; return n < 1n << 32n && d < 1n << 32n ? [n, d] : null; };
const p10 = (k) => 10n ** BigInt(k);
function value(i, p = '', r = '') {
  if (i.length + p.length + r.length > 18) return null;
  const ip = BigInt(i + p || '0');
  return r ? ok(BigInt(i + p + r) - ip, p10(p.length) * (p10(r.length) - 1n)) : ok(ip, p10(p.length));
}
function literal(s) {
  let m;
  if ((m = s.match(/^(\d{1,10})÷(\d{1,10})$/))) return BigInt(m[2]) === 0n ? null : ok(BigInt(m[1]), BigInt(m[2]));
  if ((m = s.match(/^(\d+)(?:\.(\d*)(?:\((\d+)\))?)?$/))) return value(m[1], m[2] ?? '', m[3] ?? '');
  return null;
}
function resolve(path) {
  const segs = path.split('/').filter(Boolean); let at = null, nseg = 0, root = true;
  for (const s of segs) {
    if (/^\d+$/.test(s) && (root || (nseg > 0 && nseg < 3))) {
      if (s.length > 18) return null;
      const k = root ? 0 : nseg, parts = (at?.parts ?? []).slice(0, k).concat(s);
      const v = value(parts[0], parts[1] ?? '', parts[2] ?? ''); if (!v) return null;
      at = { v, parts }; nseg = k + 1; root = false;
    } else {
      if (/^\d+$/.test(s)) return null;
      const v = literal(s); if (!v) return null;
      at = { v, parts: [] }; nseg = -1; root = false;
    }
  }
  return root ? null : at.v;
}
function decimal(n, d) {
  let d1 = d, a = 0, b = 0; while (d1 % 2n === 0n) { d1 /= 2n; a++; } while (d1 % 5n === 0n) { d1 /= 5n; b++; }
  const pre = Math.max(a, b); let per = 0;
  if (d1 > 1n) { let x = 10n % d1; per = 1; while (x !== 1n) { if (per > 1000) return null; x = x * 10n % d1; per++; } }
  let s = String(n / d), rem = n % d; if (pre + per) s += '.';
  for (let k = 0; k < pre + per; k++) { if (k === pre) s += '('; rem *= 10n; s += String(rem / d); rem %= d; }
  return per ? s + ')' : s;
}
const cases = ['1/2/34', '1/23/43', '611÷495', '1.2(34)', '0/0/142857', '0.(142857)', '1÷7', '2÷14', '0/125', '0/125/0', '1÷8',
  '0.(9)', '1', '0', '3.25', '22÷7', '355÷113', '0/0588235294117647', '1÷17', '0.(0588235294117647)', '1÷65537',
  '12/34/56/78', 'x', '1÷0', '0.(', '1.2()', '1/2/34/1÷7', '611÷495/1/2', '1/2/..', '1/2/34/../..', '99999999999/1'];
const lines = [];
for (const c of cases) {
  const v = resolve(c.replace(/(^|\/)[^/]+\/\.\.(?=\/|$)/g, (m) => m).replace(/\/\.\.$/, '') && c);
  let real = c;
  // walk .. as repfs does: a digit segment pops, anything else returns to the root
  const segs = c.split('/'), kept = [];
  for (const s of segs) { if (s === '..') { if (kept.length && /^\d+$/.test(kept.at(-1)) && kept.length > 1) kept.pop(); else kept.length = 0; } else kept.push(s); }
  real = kept.join('/');
  const r = real ? resolve(real) : null;
  if (!r) { lines.push(`${c}: refused`); continue; }
  const [n, d] = r, names = [`${n}÷${d}`, decimal(n, d)].filter(Boolean).sort((a, b) => Buffer.compare(Buffer.from(a), Buffer.from(b)));
  lines.push(`${c}: ${((n << 32n) | d).toString(16).padStart(16, '0')} ${names.join(' ')}`);
}
writeFileSync('cases', cases.join('\n') + '\n');
writeFileSync('expected', lines.join('\n') + '\n');
console.log(lines.join('\n'));
