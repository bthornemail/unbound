/*
 * zmvsurf - Metaverse Kit's mv-evidence-surface in Plan 9 C, literal: a surface on the 240 ring (a start p0, a
 * step d coprime to 240, k_max chords {p0 + kd, p0 - kd}), rendered as chords or a spiral, as LED frames (chord
 * k = t mod k_max on, the one before dim, a pointer from p0 + t or a pointer trace), as ESP32 emitter frames (on
 * the union of pointer and chord, dim the rest), as 67-byte UART packets (0x30, 0x01, t and frame_ms big-endian,
 * the pointer or 0xff, a 30-byte mask of on and one of dim, a XOR crc byte if asked), decoded back, and as the
 * wave31 decode receipt and frame verify. The tool takes p0, d and k_max from the first three bytes of a seed
 * digest, the evidence bundle's sha256, and names all that follows by digest; here the seed is the three numbers
 * (the author's choice, 2026-11-02), and the artifacts are literal: a surface holds its chords; every
 * surface_digest is seed {d, k_max, p0}, which is the surface; a packet's frame_digest is the emitter frame; a
 * receipt's packet stream and a verify's frame stream are held whole. The packet bytes and the .bin are the
 * tool's own.
 *	zmvsurf build --p0 N --d N --k-max N --out F
 *	zmvsurf verify --in F | render --in F --mode chords|spiral --out F
 *	zmvsurf render-leds240|render-leds240-esp32 --surface F --out F [--frames N] [--frame-ms N] [--pointer-trace F]
 *	zmvsurf verify-leds240|verify-leds240-esp32 --surface F --in F [--frame-ms N] [--pointer-trace F]
 *	zmvsurf emit-esp32-uart|verify-esp32-uart|decode-esp32-uart|wave31-verify --surface F --emitter F ...
 *	zmvsurf -n ...		the falsifier: verify trusts what is written (no chords, frames or packets recomputed)
 */
#include <u.h>
#include <libc.h>

enum { Jundef, Jnull, Jbool, Jnum, Jstr, Jarr, Jobj };

typedef struct J J;
struct J {
	int t;
	int b;
	double n;
	ushort *s;	/* Jstr: UTF-16 units */
	int ns;
	J **v;		/* Jarr: elements; Jobj: values */
	ushort **k;	/* Jobj: keys */
	int *nk;
	int nv;
};

static char *in, *ip;
static int catching;	/* an NDJSON line: a parse error is the caller's */
static jmp_buf caught;
static char *out;
static long nout, aout;

static void
put(char *s, int n)
{
	if(nout + n + 1 > aout){
		aout = (nout + n + 1) * 2;
		out = realloc(out, aout);
	}
	memmove(out + nout, s, n);
	nout += n;
	out[nout] = 0;
}

static void puts1(char *s) { put(s, strlen(s)); }

static J*
mk(int t)
{
	J *j;

	j = mallocz(sizeof *j, 1);
	j->t = t;
	return j;
}

static void
bad(char *m)
{
	if(catching)
		longjmp(caught, 1);
	fprint(2, "SyntaxError: %s at position %ld\n", m, (long)(ip - in));
	exits("json");
}

static void
ws(void)
{
	while(*ip == ' ' || *ip == '\t' || *ip == '\n' || *ip == '\r')
		ip++;
}

static void
addunit(J *j, int u)
{
	j->s = realloc(j->s, (j->ns + 1) * sizeof(ushort));
	j->s[j->ns++] = u;
}

static int
hex4(void)
{
	int i, c, v;

	v = 0;
	for(i = 0; i < 4; i++){
		c = *ip++;
		if(c >= '0' && c <= '9') v = v*16 + c - '0';
		else if(c >= 'a' && c <= 'f') v = v*16 + c - 'a' + 10;
		else if(c >= 'A' && c <= 'F') v = v*16 + c - 'A' + 10;
		else bad("Bad Unicode escape");
	}
	return v;
}

static int
utf8(uchar *p, long *r)	/* one character as node's utf8 decoder reads it: bad bytes give U+FFFD */
{
	int n, i;
	long c, min;

	if(p[0] < 0x80){ *r = p[0]; return 1; }
	if((p[0] & 0xE0) == 0xC0){ n = 2; c = p[0] & 0x1F; min = 0x80; }
	else if((p[0] & 0xF0) == 0xE0){ n = 3; c = p[0] & 0x0F; min = 0x800; }
	else if((p[0] & 0xF8) == 0xF0){ n = 4; c = p[0] & 0x07; min = 0x10000; }
	else { *r = 0xFFFD; return 1; }
	for(i = 1; i < n; i++){
		if((p[i] & 0xC0) != 0x80){ *r = 0xFFFD; return i; }
		c = c << 6 | (p[i] & 0x3F);
	}
	if(c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)){ *r = 0xFFFD; return n; }
	*r = c;
	return n;
}

static J*
str(void)
{
	J *j;
	long r;

	j = mk(Jstr);
	ip++;
	for(;;){
		if(*ip == 0) bad("Unterminated string in JSON");
		if(*ip == '"'){ ip++; break; }
		if((uchar)*ip < 0x20) bad("Bad control character in string literal in JSON");
		if(*ip == '\\'){
			ip++;
			switch(*ip++){
			case '"': addunit(j, '"'); break;
			case '\\': addunit(j, '\\'); break;
			case '/': addunit(j, '/'); break;
			case 'b': addunit(j, '\b'); break;
			case 'f': addunit(j, '\f'); break;
			case 'n': addunit(j, '\n'); break;
			case 'r': addunit(j, '\r'); break;
			case 't': addunit(j, '\t'); break;
			case 'u': addunit(j, hex4()); break;
			default: bad("Bad escaped character in JSON");
			}
			continue;
		}
		ip += utf8((uchar*)ip, &r);
		if(r >= 0x10000){
			addunit(j, 0xD800 + ((r - 0x10000) >> 10));
			addunit(j, 0xDC00 + ((r - 0x10000) & 0x3FF));
		}else
			addunit(j, r);
	}
	return j;
}

static J *value(void);

static int
ueq(ushort *a, int na, ushort *b, int nb)
{
	return na == nb && memcmp(a, b, na * sizeof(ushort)) == 0;
}

static J*
obj(void)
{
	J *j, *k, *v;
	int i;

	j = mk(Jobj);
	ip++;
	ws();
	if(*ip == '}'){ ip++; return j; }
	for(;;){
		ws();
		if(*ip != '"') bad("Expected property name in JSON");
		k = str();
		ws();
		if(*ip++ != ':') bad("Expected ':' after property name in JSON");
		v = value();
		for(i = 0; i < j->nv; i++)	/* a duplicate key: the last wins */
			if(ueq(j->k[i], j->nk[i], k->s, k->ns))
				break;
		if(i == j->nv){
			j->nv++;
			j->k = realloc(j->k, j->nv * sizeof(ushort*));
			j->nk = realloc(j->nk, j->nv * sizeof(int));
			j->v = realloc(j->v, j->nv * sizeof(J*));
		}
		j->k[i] = k->s;
		j->nk[i] = k->ns;
		j->v[i] = v;
		ws();
		if(*ip == ','){ ip++; continue; }
		if(*ip == '}'){ ip++; return j; }
		bad("Expected ',' or '}' after property value in JSON");
	}
}

static J*
arr(void)
{
	J *j;

	j = mk(Jarr);
	ip++;
	ws();
	if(*ip == ']'){ ip++; return j; }
	for(;;){
		j->v = realloc(j->v, (j->nv + 1) * sizeof(J*));
		j->v[j->nv++] = value();
		ws();
		if(*ip == ','){ ip++; continue; }
		if(*ip == ']'){ ip++; return j; }
		bad("Expected ',' or ']' after array element in JSON");
	}
}

static J*
num(void)
{
	char *p;
	J *j;

	p = ip;
	if(*ip == '-') ip++;
	if(*ip == '0') ip++;
	else if(*ip >= '1' && *ip <= '9') while(*ip >= '0' && *ip <= '9') ip++;
	else bad("No number after minus sign in JSON");
	if(*ip == '.'){
		ip++;
		if(*ip < '0' || *ip > '9') bad("Unterminated fractional number in JSON");
		while(*ip >= '0' && *ip <= '9') ip++;
	}
	if(*ip == 'e' || *ip == 'E'){
		ip++;
		if(*ip == '+' || *ip == '-') ip++;
		if(*ip < '0' || *ip > '9') bad("Exponent part is missing a number in JSON");
		while(*ip >= '0' && *ip <= '9') ip++;
	}
	j = mk(Jnum);
	j->n = strtod(p, nil);
	return j;
}

static J*
word(char *w, J *j)
{
	if(strncmp(ip, w, strlen(w)) != 0) bad("Unexpected token in JSON");
	ip += strlen(w);
	return j;
}

static J*
value(void)
{
	J *j;

	ws();
	switch(*ip){
	case '{': return obj();
	case '[': return arr();
	case '"': return str();
	case 't': j = mk(Jbool); j->b = 1; return word("true", j);
	case 'f': return word("false", mk(Jbool));
	case 'n': return word("null", mk(Jnull));
	}
	if(*ip == '-' || (*ip >= '0' && *ip <= '9'))
		return num();
	bad("Unexpected token in JSON");
	return nil;
}

/*
 * the double's exact decimal digits: d = m * 2^e, m an integer of 53 bits; for e < 0, m * 5^-e over 10^-e.
 * In base 10^9, little end first. Plan 9's %e is not exact in the last digits (2^53 at 16 digits gives
 * ...993, where the value is ...992), so the digits come from here and strtod only reads them back.
 */
static int
exact(double d, char *dig, int *n)	/* digits into dig, the value 0.dig * 10^n; returns their count */
{
	static uint big[200];
	int nb, i, x, e, k, l;
	uvlong m, c;
	char *t;

	m = (uvlong)ldexp(frexp(d, &x), 53);
	e = x - 53;
	big[0] = m % 1000000000;
	big[1] = m / 1000000000 % 1000000000;
	big[2] = m / 1000000000 / 1000000000;
	nb = 3;
	for(k = 0; k < (e < 0 ? -e : e); k++){
		c = 0;
		for(i = 0; i < nb; i++){
			c += (uvlong)big[i] * (e < 0 ? 5 : 2);
			big[i] = c % 1000000000;
			c /= 1000000000;
		}
		if(c)
			big[nb++] = c;
	}
	while(nb > 1 && big[nb-1] == 0)
		nb--;
	t = dig;
	t += sprint(t, "%d", (int)big[nb-1]);
	for(i = nb - 2; i >= 0; i--)
		t += sprint(t, "%09d", (int)big[i]);
	l = t - dig;
	*n = l + (e < 0 ? e : 0);
	return l;
}

/* Number's toString: the fewest digits that read back as the double, nearest it, placed as ECMA-262 places them */
static char*
jsnum(double d, char *buf)
{
	static char x[1200];
	char e[64], dig[32];
	int p, k, n, i, l, nx, up;
	char *q;

	if(d != d){ strcpy(buf, "NaN"); return buf; }
	if(d == 0){ strcpy(buf, "0"); return buf; }
	if(d > 1.7976931348623157e308){ strcpy(buf, "Infinity"); return buf; }
	if(d < -1.7976931348623157e308){ strcpy(buf, "-Infinity"); return buf; }
	q = buf;
	if(d < 0){ *q++ = '-'; d = -d; }
	l = exact(d, x, &nx);
	for(p = 1; p <= 17; p++){
		n = nx;
		k = p < l ? p : l;
		memmove(dig, x, k);
		up = 0;
		if(p < l){
			up = x[p] > '5';
			if(x[p] == '5'){	/* a tie only when every digit after is 0: then to even */
				for(i = p + 1; i < l && x[i] == '0'; i++)
					;
				up = i < l || (x[p-1] - '0') % 2;
			}
		}
		for(i = k - 1; up && i >= 0; i--){
			if(dig[i] == '9')
				dig[i] = '0';
			else {
				dig[i]++;
				up = 0;
			}
		}
		if(up){	/* 99..9 carried: 10..0 */
			memmove(dig + 1, dig, k - 1);
			dig[0] = '1';
			n++;
		}
		dig[k] = 0;
		snprint(e, sizeof e, "%c.%se%d", dig[0], dig + 1, n - 1);
		if(strtod(e, nil) == d)
			break;
	}
	while(k > 1 && dig[k-1] == '0')
		k--;
	dig[k] = 0;
	if(k <= n && n <= 21){
		q += sprint(q, "%s", dig);
		for(i = k; i < n; i++) *q++ = '0';
		*q = 0;
	}else if(0 < n && n <= 21)
		sprint(q, "%.*s.%s", n, dig, dig + n);
	else if(-6 < n && n <= 0){
		q += sprint(q, "0.");
		for(i = n; i < 0; i++) *q++ = '0';
		sprint(q, "%s", dig);
	}else if(k == 1)
		sprint(q, "%se%c%d", dig, n - 1 < 0 ? '-' : '+', n - 1 < 0 ? 1 - n : n - 1);
	else
		sprint(q, "%c.%se%c%d", dig[0], dig + 1, n - 1 < 0 ? '-' : '+', n - 1 < 0 ? 1 - n : n - 1);
	return buf;
}

static void
putunits(ushort *s, int n, int quote)	/* UTF-8, and with quote as JSON.stringify writes a string: a lone surrogate escaped */
{
	char b[16];
	long r;
	int i, m;
	Rune rr;

	if(quote) put("\"", 1);
	for(i = 0; i < n; i++){
		r = s[i];
		if(quote) switch(r){
		case '"': puts1("\\\""); continue;
		case '\\': puts1("\\\\"); continue;
		case '\b': puts1("\\b"); continue;
		case '\f': puts1("\\f"); continue;
		case '\n': puts1("\\n"); continue;
		case '\r': puts1("\\r"); continue;
		case '\t': puts1("\\t"); continue;
		}
		if(quote && r < 0x20){
			snprint(b, sizeof b, "\\u%04lx", r);
			puts1(b);
			continue;
		}
		if(r >= 0xD800 && r <= 0xDBFF && i + 1 < n && s[i+1] >= 0xDC00 && s[i+1] <= 0xDFFF){
			r = 0x10000 + ((r - 0xD800) << 10) + (s[i+1] - 0xDC00);
			i++;
		}else if(r >= 0xD800 && r <= 0xDFFF){
			snprint(b, sizeof b, "\\u%04lx", r);
			puts1(b);
			continue;
		}
		rr = r;
		m = runetochar(b, &rr);
		put(b, m);
	}
	if(quote) put("\"", 1);
}

static void
putlit(char *s)	/* an ASCII string, quoted */
{
	put("\"", 1);
	puts1(s);
	put("\"", 1);
}

static int
ucmp(ushort *a, int na, ushort *b, int nb)	/* as Array.prototype.sort compares strings */
{
	int i;

	for(i = 0; i < na && i < nb; i++)
		if(a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	return na < nb ? -1 : na > nb;
}


static int trust;

static void
die(char *m)
{
	fprint(2, "ERROR: %s\n", m);
	exits("2");
}

static void
stable(J *j)	/* the tools' stableStringify */
{
	char b[64];
	int i, m, *ord, t;

	switch(j->t){
	case Jnull: puts1("null"); return;
	case Jbool: puts1(j->b ? "true" : "false"); return;
	case Jnum:
		jsnum(j->n, b);
		puts1(strchr(b, 'I') || strchr(b, 'N') ? "null" : b);
		return;
	case Jstr: putunits(j->s, j->ns, 1); return;
	case Jarr:
		put("[", 1);
		for(i = 0; i < j->nv; i++){
			if(i) put(",", 1);
			stable(j->v[i]);
		}
		put("]", 1);
		return;
	}
	ord = malloc((j->nv + 1) * sizeof(int));
	for(i = 0; i < j->nv; i++)
		ord[i] = i;
	for(i = 1; i < j->nv; i++)
		for(m = i; m > 0 && ucmp(j->k[ord[m]], j->nk[ord[m]], j->k[ord[m-1]], j->nk[ord[m-1]]) < 0; m--){
			t = ord[m]; ord[m] = ord[m-1]; ord[m-1] = t;
		}
	put("{", 1);
	for(i = 0; i < j->nv; i++){
		if(i) put(",", 1);
		putunits(j->k[ord[i]], j->nk[ord[i]], 1);
		put(":", 1);
		stable(j->v[ord[i]]);
	}
	put("}", 1);
	free(ord);
}

static char*
canon(J *j)	/* canonicalJson: stableStringify and a newline */
{
	long s;
	char *r;

	s = nout;
	stable(j);
	put("\n", 1);
	r = malloc(nout - s + 1);
	memmove(r, out + s, nout - s);
	r[nout - s] = 0;
	nout = s;
	out[nout] = 0;
	return r;
}

static int
same(J *a, J *b)
{
	char *x, *y;
	int r;

	x = canon(a);
	y = canon(b);
	r = strcmp(x, y) == 0;
	free(x);
	free(y);
	return r;
}

static void
addutf(J *j, char *s)	/* UTF-8 onto j's units */
{
	long r;

	while(*s){
		s += utf8((uchar*)s, &r);
		if(r >= 0x10000){
			addunit(j, 0xD800 + ((r - 0x10000) >> 10));
			addunit(j, 0xDC00 + ((r - 0x10000) & 0x3FF));
		}else
			addunit(j, r);
	}
}

static J*
mkstr(char *s)
{
	J *j;

	j = mk(Jstr);
	addutf(j, s);
	return j;
}

static int
cmp16(char *a, char *b)	/* two strings as JavaScript compares them, by UTF-16 code units */
{
	J *x, *y;

	x = mkstr(a);
	y = mkstr(b);
	return ucmp(x->s, x->ns, y->s, y->ns);
}

static char*
utf(ushort *s, int n)	/* units to UTF-8 */
{
	long st;
	char *r;
	int i;
	Rune rr;
	char b[8];

	st = nout;
	for(i = 0; i < n; i++){
		if(s[i] >= 0xD800 && s[i] <= 0xDBFF && i + 1 < n && s[i+1] >= 0xDC00 && s[i+1] <= 0xDFFF){
			rr = 0x10000 + ((s[i] - 0xD800) << 10) + (s[i+1] - 0xDC00);
			i++;
		}else
			rr = s[i];
		put(b, runetochar(b, &rr));
	}
	r = malloc(nout - st + 1);
	memmove(r, out + st, nout - st);
	r[nout - st] = 0;
	nout = st;
	if(out) out[nout] = 0;
	return r;
}

static int
eqs(J *j, char *s)	/* j a string, s */
{
	char *u;
	int r;

	if(j == nil || j->t != Jstr)
		return 0;
	u = utf(j->s, j->ns);
	r = strcmp(u, s) == 0;
	free(u);
	return r;
}

static J*
get(J *o, char *key)	/* nil: undefined */
{
	int i;
	char *k;

	if(o == nil || o->t != Jobj)
		return nil;
	for(i = 0; i < o->nv; i++){
		k = utf(o->k[i], o->nk[i]);
		if(strcmp(k, key) == 0){
			free(k);
			return o->v[i];
		}
		free(k);
	}
	return nil;
}

static void
set(J *o, char *key, J *v)
{
	J *k;

	k = mkstr(key);
	o->nv++;
	o->k = realloc(o->k, o->nv * sizeof(ushort*));
	o->nk = realloc(o->nk, o->nv * sizeof(int));
	o->v = realloc(o->v, o->nv * sizeof(J*));
	o->k[o->nv-1] = k->s;
	o->nk[o->nv-1] = k->ns;
	o->v[o->nv-1] = v;
}

static void
push(J *a, J *v)
{
	a->v = realloc(a->v, (a->nv + 1) * sizeof(J*));
	a->v[a->nv++] = v;
}

static int
sortstr(void *a, void *b)
{
	return cmp16(*(char**)a, *(char**)b);
}

static void
keyset(J *o, char **want, char *c)	/* Object.keys(o) sorted and joined, against want's */
{
	char *got[512], *w[64], *g, *x;
	int n, nw, i;
	long len;

	if(o == nil || o->t == Jnull)
		die("Cannot convert undefined or null to object");
	n = 0;
	if(o->t == Jobj)
		for(i = 0; i < o->nv && n < nelem(got); i++)
			got[n++] = utf(o->k[i], o->nk[i]);
	else if(o->t == Jarr || o->t == Jstr)
		for(i = 0; i < (o->t == Jarr ? o->nv : o->ns) && n < nelem(got); i++)
			got[n++] = smprint("%d", i);
	for(nw = 0; want[nw]; nw++)
		w[nw] = want[nw];
	qsort(got, n, sizeof(char*), sortstr);
	qsort(w, nw, sizeof(char*), sortstr);
	len = 1;
	for(i = 0; i < n; i++)
		len += strlen(got[i]) + 1;
	g = mallocz(len, 1);
	for(i = 0; i < n; i++){
		if(i) strcat(g, ",");
		strcat(g, got[i]);
	}
	len = 1;
	for(i = 0; i < nw; i++)
		len += strlen(w[i]) + 1;
	x = mallocz(len, 1);
	for(i = 0; i < nw; i++){
		if(i) strcat(x, ",");
		strcat(x, w[i]);
	}
	if(strcmp(g, x) != 0)
		die(smprint("%s keyset mismatch", c));
}

static void
reqsha(J *v, char *c)	/* /^sha256:[0-9a-f]{64}$/ */
{
	int i, ok;

	ok = v != nil && v->t == Jstr && v->ns == 71;
	for(i = 0; ok && i < 7; i++)
		ok = v->s[i] == "sha256:"[i];
	for(i = 7; ok && i < 71; i++)
		ok = (v->s[i] >= '0' && v->s[i] <= '9') || (v->s[i] >= 'a' && v->s[i] <= 'f');
	if(!ok)
		die(smprint("%s invalid sha256", c));
}

static char *noembedded[] = { nil };
static char **embedded = noembedded;	/* the literals of an artifact's inputs, which the string membrane does not cover */

static int
isindex(ushort *s, int n)	/* an array index key, which Object.entries puts first */
{
	int i;
	double v;

	if(n == 0 || n > 10 || (n > 1 && s[0] == '0'))
		return 0;
	v = 0;
	for(i = 0; i < n; i++){
		if(s[i] < '0' || s[i] > '9')
			return 0;
		v = v*10 + s[i] - '0';
	}
	return v < 4294967295.0;
}

static double
indexval(ushort *s, int n)
{
	double v;
	int i;

	v = 0;
	for(i = 0; i < n; i++)
		v = v*10 + s[i] - '0';
	return v;
}

static void
reqmem(J *v, char *c)	/* the string membrane, over all the artifact holds but the literals of its inputs */
{
	int i, m, *ord, t, e;
	char *k;

	if(v->t == Jarr){
		for(i = 0; i < v->nv; i++)
			reqmem(v->v[i], smprint("%s[%d]", c, i));
		return;
	}
	if(v->t == Jobj){
		ord = malloc((v->nv + 1) * sizeof(int));
		m = 0;
		for(i = 0; i < v->nv; i++)
			if(isindex(v->k[i], v->nk[i]))
				ord[m++] = i;
		for(i = 1; i < m; i++)
			for(e = i; e > 0 && indexval(v->k[ord[e]], v->nk[ord[e]]) < indexval(v->k[ord[e-1]], v->nk[ord[e-1]]); e--){
				t = ord[e]; ord[e] = ord[e-1]; ord[e-1] = t;
			}
		for(i = 0; i < v->nv; i++)
			if(!isindex(v->k[i], v->nk[i]))
				ord[m++] = i;
		for(i = 0; i < v->nv; i++){
			k = utf(v->k[ord[i]], v->nk[ord[i]]);
			for(e = 0; embedded[e] && strcmp(embedded[e], k) != 0; e++)
				;
			if(embedded[e] == nil)
				reqmem(v->v[ord[i]], smprint("%s.%s", c, k));
		}
		free(ord);
		return;
	}
	if(v->t != Jstr)
		die(smprint("%s violates string membrane", c));
}

static int
digits(J *v, int sign)	/* /^[0-9]+$/, or with sign /^-?[0-9]+$/ */
{
	int i;

	if(v == nil || v->t != Jstr)
		return 0;
	i = sign && v->ns > 0 && v->s[0] == '-';
	if(i == v->ns)
		return 0;
	for(; i < v->ns; i++)
		if(v->s[i] < '0' || v->s[i] > '9')
			return 0;
	return 1;
}

static double
number(J *v)	/* Number(s) */
{
	char *s;
	double d;

	s = utf(v->s, v->ns);
	if(strcmp(s, "Infinity") == 0) d = Inf(1);
	else if(strcmp(s, "-Infinity") == 0) d = Inf(-1);
	else if(strcmp(s, "NaN") == 0) d = NaN();
	else d = strtod(s, nil);
	free(s);
	return d;
}

static double
udec(J *v, char *c)
{
	if(!digits(v, 0))
		die(smprint("%s must be unsigned decimal string", c));
	return number(v);
}

static void
idec(J *v, char *c)
{
	if(!digits(v, 1))
		die(smprint("%s must be integer string", c));
}

static J*
numstr(double d)	/* String(d) */
{
	char b[64];

	return mkstr(jsnum(d, b));
}

static J*
readjson(char *f)
{
	int fd;
	long n, len;
	char *s, buf[8192];
	J *j;

	if((fd = open(f, OREAD)) < 0)
		die(smprint("ENOENT: no such file or directory, open '%s'", f));
	s = strdup("");
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, len + n + 1);
		memmove(s + len, buf, n);
		len += n;
		s[len] = 0;
	}
	close(fd);
	in = ip = s;
	j = value();
	ws();
	if(*ip)
		bad("Unexpected non-whitespace character after JSON");
	return j;
}

static void
writejson(char *f, J *j)
{
	int fd;
	char *s;

	s = canon(j);
	if((fd = create(f, OWRITE, 0666)) < 0)
		die(smprint("cannot create %s", f));
	write(fd, s, strlen(s));
	close(fd);
	free(s);
}

static int
oneof(J *v, char **set)
{
	int i;

	for(i = 0; set[i]; i++)
		if(eqs(v, set[i]))
			return 1;
	return 0;
}

/* ── the arguments, as each tool's parseArgs takes them: a flag takes the next when it is not empty ── */

typedef struct Arg Arg;
struct Arg {
	char *mode;
	char *flag[8];
	char *val[8];
	int help;
};

static void
args(Arg *a, int argc, char **argv, char **flags)
{
	int i, f;

	memset(a, 0, sizeof *a);
	for(i = 0; i < argc; i++){
		if(strcmp(argv[i], "emit") == 0){ a->mode = "emit"; continue; }
		if(strcmp(argv[i], "validate") == 0){ a->mode = "validate"; continue; }
		for(f = 0; flags[f]; f++)
			if(strcmp(argv[i], flags[f]) == 0 && i + 1 < argc && argv[i+1][0])
				break;
		if(flags[f]){
			a->val[f] = argv[++i];
			continue;
		}
		if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0){ a->help = 1; continue; }
		die(smprint("unknown arg: %s", argv[i]));
	}
}


enum { RING = 240, KLIMIT = 48, PKT = 67 };
#define FRAME_V "wave30.evidence_surface_frame.v0"
#define EMITTER_V "wave30.evidence_surface_emitter_frame.esp32.v0"
#define UART_V "wave30.evidence_surface_uart_packet.esp32.v0"
#define CRC_ID "crc8-xor-v0"

static char *packetskip[] = { "frame", nil };

static char*
sval(J *v)
{
	return utf(v->s, v->ns);
}

static int
truthy(J *j)
{
	if(j == nil) return 0;
	switch(j->t){
	case Jnull: return 0;
	case Jbool: return j->b;
	case Jnum: return j->n != 0 && j->n == j->n;
	case Jstr: return j->ns > 0;
	}
	return 1;
}

static J*
newarr(void)
{
	return mk(Jarr);
}

static J*
nstr(long n)	/* String(n) */
{
	return mkstr(smprint("%ld", n));
}

static long
toint(J *s, long min, long max, char *ctx)	/* toIntInRange */
{
	double n;

	if(!digits(s, 0)) die(smprint("%s invalid integer string", ctx));
	n = number(s);
	if(n != floor(n) || n < min || n > max) die(smprint("%s out of range", ctx));
	return (long)n;
}

static long
gcd(long a, long b)
{
	long t;

	while(b){
		t = a % b;
		a = b;
		b = t;
	}
	return a;
}

/* ── reading and writing ── */

static J*
readnd(char *f)	/* readNdjson: ends with a newline; the lines of its trimEnd, each JSON */
{
	int fd, line;
	long n, len;
	char *s, buf[8192], *p, *nl;
	J *rows;

	if((fd = open(f, OREAD)) < 0)
		die(smprint("ENOENT: no such file or directory, open '%s'", f));
	s = strdup("");
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, len + n + 1);
		memmove(s + len, buf, n);
		len += n;
		s[len] = 0;
	}
	close(fd);
	if(len == 0 || s[len-1] != '\n') die("NDJSON must end with newline");
	while(len > 0 && strchr(" \t\n\r\v\f", s[len-1]))
		s[--len] = 0;
	rows = newarr();
	line = 0;
	for(p = s;; p = nl + 1){
		line++;
		nl = strchr(p, '\n');
		if(nl) *nl = 0;
		in = ip = p;
		catching = 1;
		if(setjmp(caught)){
			catching = 0;
			die(smprint("invalid NDJSON line %d", line));
		}
		push(rows, value());
		ws();
		if(*ip) bad("Unexpected non-whitespace character after JSON");
		catching = 0;
		if(nl == nil)
			break;
	}
	return rows;
}

static void
writend(char *f, J *rows)
{
	int fd, i;
	char *s;

	if((fd = create(f, OWRITE, 0666)) < 0)
		die(smprint("cannot create %s", f));
	for(i = 0; i < rows->nv; i++){
		s = canon(rows->v[i]);
		write(fd, s, strlen(s));
		free(s);
	}
	close(fd);
}

typedef struct Buf Buf;
struct Buf { uchar *b; long n; };

static Buf
readbin(char *f)
{
	Buf r;
	int fd;
	long n;
	uchar buf[8192];

	if((fd = open(f, OREAD)) < 0)
		die(smprint("ENOENT: no such file or directory, open '%s'", f));
	r.b = nil;
	r.n = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		r.b = realloc(r.b, r.n + n);
		memmove(r.b + r.n, buf, n);
		r.n += n;
	}
	close(fd);
	return r;
}

/* ── the surface ── */

typedef struct Surf Surf;
struct Surf { long p0, d, k; J *j; };

static J*
seedof(J *s)	/* the surface's seed: {d, k_max, p0} */
{
	J *x;

	x = mk(Jobj);
	set(x, "d", get(s, "d"));
	set(x, "k_max", get(s, "k_max"));
	set(x, "p0", get(s, "p0"));
	return x;
}

static J*
pair(long a, long b)
{
	J *x;

	x = mk(Jobj);
	set(x, "lo", nstr(a < b ? a : b));
	set(x, "hi", nstr(a < b ? b : a));
	return x;
}

static J*
chordsbyk(long p0, long d, long k)
{
	J *out;
	long i, a, b;

	out = newarr();
	for(i = 1; i <= k; i++){
		a = (p0 + i * d) % RING;
		b = (p0 - i * d + RING * 1000) % RING;
		if(a == b) die("surface chord degenerate");
		push(out, pair(a, b));
	}
	return out;
}

static int
chordcmp(J *x, J *y)	/* lo, then hi, as strings (digits: localeCompare is their code units) */
{
	if(same(get(x, "lo"), get(y, "lo")))
		return cmp16(sval(get(x, "hi")), sval(get(y, "hi")));
	return cmp16(sval(get(x, "lo")), sval(get(y, "lo")));
}

static J*
chords(long p0, long d, long k)	/* chordPairsCanonical */
{
	J *c, *t;
	int i, m;

	c = chordsbyk(p0, d, k);
	for(i = 1; i < c->nv; i++)
		for(m = i; m > 0 && chordcmp(c->v[m], c->v[m-1]) < 0; m--){
			t = c->v[m]; c->v[m] = c->v[m-1]; c->v[m-1] = t;
		}
	for(i = 1; i < c->nv; i++)
		for(m = 0; m < i; m++)
			if(same(c->v[i], c->v[m])) die("surface chords duplicate");
	return c;
}

static char *SURFKEYS[] = { "authority", "chords", "d", "k_max", "p0", "ring_size", "v", nil };

static void
params(J *x, Surf *s)	/* the ranges and the step's coprimality */
{
	toint(get(x, "ring_size"), RING, RING, "evidence_surface.ring_size");
	s->p0 = toint(get(x, "p0"), 0, RING - 1, "evidence_surface.p0");
	s->d = toint(get(x, "d"), 1, 119, "evidence_surface.d");
	s->k = toint(get(x, "k_max"), 1, KLIMIT, "evidence_surface.k_max");
	if(!trust && gcd(s->d, RING) != 1) die("evidence_surface.d must be coprime to 240");
}

static Surf
validatesurface(J *x)
{
	Surf s;

	keyset(x, SURFKEYS, "evidence_surface");
	if(!eqs(get(x, "v"), "wave30.evidence_surface.chords.v0")) die("evidence_surface version mismatch");
	if(!eqs(get(x, "authority"), "advisory")) die("evidence_surface authority must be advisory");
	params(x, &s);
	if(!trust && !same(chords(s.p0, s.d, s.k), get(x, "chords"))) die("evidence_surface chords mismatch");
	embedded = noembedded;
	reqmem(x, "evidence_surface");
	s.j = x;
	return s;
}

static Surf
readsurface(char *f)
{
	return validatesurface(readjson(f));
}

/* ── frames ── */

static int
numcmp(void *a, void *b)
{
	double x, y;

	x = number(*(J**)a);
	y = number(*(J**)b);
	return x < y ? -1 : x > y;
}

static J*
sortidx(J *a)	/* by Number, stable */
{
	J *s, *t;
	int i, m;

	s = newarr();
	for(i = 0; i < a->nv; i++)
		push(s, a->v[i]);
	for(i = 1; i < s->nv; i++)
		for(m = i; m > 0 && numcmp(&s->v[m], &s->v[m-1]) < 0; m--){
			t = s->v[m]; s->v[m] = s->v[m-1]; s->v[m-1] = t;
		}
	return s;
}

static int
inset(J *a, J *x)	/* a Set of strings has x */
{
	int i;

	for(i = 0; i < a->nv; i++)
		if(same(a->v[i], x))
			return 1;
	return 0;
}

static J*
sched;	/* the pointer schedule, or nil */

static void
readsched(char *f)
{
	J *rows, *r;
	int i;

	rows = readnd(f);
	sched = newarr();
	for(i = 0; i < rows->nv; i++){
		r = rows->v[i];
		if(!truthy(r) || (r->t != Jobj && r->t != Jarr)) die(smprint("pointer trace row %d invalid", i + 1));
		push(sched, nstr(toint(get(r, "p_after"), 0, RING - 1, smprint("pointer_trace[%d].p_after", i))));
	}
}

static J*
frame(Surf *s, long t)	/* expectedFrame */
{
	J *byk, *f, *on, *dim, *ptr, *c, *pc;
	long idx, prev;

	byk = chordsbyk(s->p0, s->d, s->k);
	idx = t % s->k;
	prev = (t + s->k - 1) % s->k;
	c = byk->v[idx];
	on = newarr();
	push(on, get(c, "lo"));
	push(on, get(c, "hi"));
	dim = newarr();
	if(t != 0){
		pc = byk->v[prev];
		push(dim, get(pc, "lo"));
		push(dim, get(pc, "hi"));
	}
	ptr = newarr();
	push(ptr, sched && sched->nv > 0 ? sched->v[t % sched->nv] : nstr((s->p0 + t) % RING));
	f = mk(Jobj);
	set(f, "authority", mkstr("advisory"));
	set(f, "chord_dim", sortidx(dim));
	set(f, "chord_on", sortidx(on));
	set(f, "mode", mkstr("leds240"));
	set(f, "pointer_on", ptr);
	set(f, "ring_size", mkstr("240"));
	set(f, "seed", seedof(s->j));
	set(f, "t", nstr(t));
	set(f, "v", mkstr(FRAME_V));
	return f;
}

static J*
emitter(Surf *s, long t, long ms)	/* expectedEmitterFrame */
{
	J *b, *on, *dim, *u, *x, *e;
	int i;

	b = frame(s, t);
	u = newarr();
	x = get(b, "pointer_on");
	for(i = 0; i < x->nv; i++) if(!inset(u, x->v[i])) push(u, x->v[i]);
	x = get(b, "chord_on");
	for(i = 0; i < x->nv; i++) if(!inset(u, x->v[i])) push(u, x->v[i]);
	on = sortidx(u);
	dim = newarr();
	x = get(b, "chord_dim");
	for(i = 0; i < x->nv; i++) if(!inset(on, x->v[i])) push(dim, x->v[i]);
	e = mk(Jobj);
	set(e, "authority", mkstr("advisory"));
	set(e, "dim", dim);
	set(e, "frame_ms", nstr(ms));
	set(e, "mode", mkstr("leds240"));
	set(e, "on", on);
	set(e, "pointer", get(b, "pointer_on"));
	set(e, "profile", mkstr("esp32.v0"));
	set(e, "ring_size", mkstr("240"));
	set(e, "seed", get(b, "seed"));
	set(e, "t", nstr(t));
	set(e, "v", mkstr(EMITTER_V));
	return e;
}

static void
idxarray(J *a, char *ctx)	/* validateIdxArray */
{
	int i, k;
	char *x, *y;

	if(a->t != Jarr) die(smprint("%s must be array", ctx));
	for(i = 0; i < a->nv; i++)
		toint(a->v[i], 0, RING - 1, smprint("%s[%d]", ctx, i));
	x = canon(a);
	y = canon(sortidx(a));
	if(strcmp(x, y) != 0) die(smprint("%s must be sorted ascending", ctx));
	for(i = 0; i < a->nv; i++)
		for(k = 0; k < i; k++)
			if(same(a->v[i], a->v[k])) die(smprint("%s contains duplicates", ctx));
}

static char *FRAMEKEYS[] = { "authority", "chord_dim", "chord_on", "mode", "pointer_on", "ring_size", "seed", "t", "v", nil };
static char *EMITKEYS[] = { "authority", "dim", "frame_ms", "mode", "on", "pointer", "profile", "ring_size", "seed", "t", "v", nil };

static void
validateframe(J *f)
{
	J *on, *dim;
	int i;

	keyset(f, FRAMEKEYS, "surface_frame");
	if(!eqs(get(f, "v"), FRAME_V)) die("surface_frame version mismatch");
	if(!eqs(get(f, "authority"), "advisory")) die("surface_frame authority must be advisory");
	if(!eqs(get(f, "mode"), "leds240")) die("surface_frame mode mismatch");
	if(!eqs(get(f, "ring_size"), "240")) die("surface_frame ring_size mismatch");
	toint(get(f, "t"), 0, 1000000000, "surface_frame.t");
	idxarray(get(f, "pointer_on"), "surface_frame.pointer_on");
	idxarray(on = get(f, "chord_on"), "surface_frame.chord_on");
	idxarray(dim = get(f, "chord_dim"), "surface_frame.chord_dim");
	for(i = 0; i < dim->nv; i++)
		if(inset(on, dim->v[i])) die("surface_frame chord_on intersects chord_dim");
	embedded = noembedded;
	reqmem(f, "surface_frame");
}

static void
validateemitter(J *f)
{
	J *on, *dim, *p;
	int i;

	keyset(f, EMITKEYS, "surface_emitter_frame");
	if(!eqs(get(f, "v"), EMITTER_V)) die("surface_emitter_frame version mismatch");
	if(!eqs(get(f, "authority"), "advisory")) die("surface_emitter_frame authority must be advisory");
	if(!eqs(get(f, "mode"), "leds240")) die("surface_emitter_frame mode mismatch");
	if(!eqs(get(f, "profile"), "esp32.v0")) die("surface_emitter_frame profile mismatch");
	if(!eqs(get(f, "ring_size"), "240")) die("surface_emitter_frame ring_size mismatch");
	toint(get(f, "t"), 0, 1000000000, "surface_emitter_frame.t");
	toint(get(f, "frame_ms"), 1, 5000, "surface_emitter_frame.frame_ms");
	idxarray(on = get(f, "on"), "surface_emitter_frame.on");
	idxarray(dim = get(f, "dim"), "surface_emitter_frame.dim");
	idxarray(p = get(f, "pointer"), "surface_emitter_frame.pointer");
	if(p->nv > 1) die("surface_emitter_frame.pointer max length is 1");
	for(i = 0; i < p->nv; i++)
		if(!inset(on, p->v[i])) die("surface_emitter_frame.pointer must be subset of on");
	for(i = 0; i < dim->nv; i++)
		if(inset(on, dim->v[i])) die("surface_emitter_frame on intersects dim");
	embedded = noembedded;
	reqmem(f, "surface_emitter_frame");
}

/* ── packets ── */

static void
mask(uchar *m, J *a)
{
	int i;
	long x;

	memset(m, 0, 30);
	for(i = 0; i < a->nv; i++){
		x = (long)number(a->v[i]);
		m[x / 8] |= 1 << (x % 8);
	}
}

static J*
unmask(uchar *m)
{
	J *a;
	int i;

	a = newarr();
	for(i = 0; i < RING; i++)
		if(m[i / 8] & (1 << (i % 8)))
			push(a, nstr(i));
	return a;
}

static int
pack(J *f, int crc, uchar *b)	/* packEmitterFrame: the packet's length */
{
	long t, ms;
	J *p;
	int i, n;

	n = PKT + crc;
	memset(b, 0, n);
	t = (long)number(get(f, "t"));
	ms = (long)number(get(f, "frame_ms"));
	p = get(f, "pointer");
	b[0] = 0x30;
	b[1] = 0x01;
	b[2] = (t >> 8) & 0xff;
	b[3] = t & 0xff;
	b[4] = (ms >> 8) & 0xff;
	b[5] = ms & 0xff;
	b[6] = p->nv == 1 ? (long)number(p->v[0]) : 0xff;
	mask(b + 7, get(f, "on"));
	mask(b + 37, get(f, "dim"));
	if(crc){
		b[PKT] = 0;
		for(i = 0; i < PKT; i++)
			b[PKT] ^= b[i];
	}
	return n;
}

static char*
hex(uchar *b, int n)
{
	char *s;
	int i;

	s = malloc(2 * n + 1);
	for(i = 0; i < n; i++)
		sprint(s + 2 * i, "%02x", b[i]);
	s[2 * n] = 0;
	return s;
}

static J*
record(Surf *s, J *f, char *crc)	/* buildUartPacketRecord */
{
	uchar b[PKT + 1];
	int n;
	J *r;

	n = pack(f, strcmp(crc, CRC_ID) == 0, b);
	r = mk(Jobj);
	set(r, "authority", mkstr("advisory"));
	set(r, "frame", f);
	set(r, "packet_bytes", nstr(n));
	set(r, "packet_hex", mkstr(hex(b, n)));
	set(r, "profile", mkstr("esp32.uart.v0"));
	set(r, "seed", seedof(s->j));
	set(r, "t", get(f, "t"));
	set(r, "uart_crc", mkstr(crc));
	set(r, "v", mkstr(UART_V));
	return r;
}

static int
hexdigits(J *x)	/* /^[0-9a-f]+$/ */
{
	int i;

	if(x == nil || x->t != Jstr || x->ns == 0) return 0;
	for(i = 0; i < x->ns; i++)
		if(!((x->s[i] >= '0' && x->s[i] <= '9') || (x->s[i] >= 'a' && x->s[i] <= 'f')))
			return 0;
	return 1;
}

static char hexd[] = "0123456789abcdef";

static void
unhex(J *x, uchar *b)
{
	int i;
	char *s;

	s = sval(x);	/* lower-case hex, checked before */
	for(i = 0; s[2*i]; i++)
		b[i] = (strchr(hexd, s[2*i]) - hexd) << 4 | (strchr(hexd, s[2*i+1]) - hexd);
}

static char *PACKETKEYS[] = { "authority", "frame", "packet_bytes", "packet_hex", "profile", "seed", "t", "uart_crc", "v", nil };

static void
validaterecord(J *x)
{
	int want, i, c;
	uchar b[PKT + 1];
	J *h;

	keyset(x, PACKETKEYS, "surface_uart_packet");
	if(!eqs(get(x, "v"), UART_V)) die("surface_uart_packet version mismatch");
	if(!eqs(get(x, "authority"), "advisory")) die("surface_uart_packet authority must be advisory");
	if(!eqs(get(x, "profile"), "esp32.uart.v0")) die("surface_uart_packet profile mismatch");
	if(!eqs(get(x, "uart_crc"), "none") && !eqs(get(x, "uart_crc"), CRC_ID)) die("surface_uart_packet uart_crc unknown");
	want = PKT + eqs(get(x, "uart_crc"), CRC_ID);
	if(!same(get(x, "packet_bytes"), nstr(want))) die("surface_uart_packet packet_bytes mismatch");
	toint(get(x, "t"), 0, 1000000000, "surface_uart_packet.t");
	h = get(x, "packet_hex");
	if(!hexdigits(h)) die("surface_uart_packet packet_hex invalid");
	if(h->ns != want * 2) die("surface_uart_packet packet_hex length mismatch");
	if(want > PKT){
		unhex(h, b);
		for(i = c = 0; i < PKT; i++)
			c ^= b[i];
		if(b[PKT] != c) die("surface_uart_packet crc mismatch");
	}
	embedded = packetskip;
	reqmem(x, "surface_uart_packet");
}

/* ── the commands ── */

static char *mode, *seedarg[3], *inf, *surface, *emitterf, *outf, *outbin, *inbin, *receiptout, *verifyout, *trace;
static char *frames = "12", *framems = "50", *crc = "none", *rmode = "chords";

static void
usage(void)
{
	print("mv-evidence-surface build --p0 <0..239> --d <1..119> --k-max <1..48> --out <surface.json>\n");
	print("mv-evidence-surface verify --in <surface.json>\n");
	print("mv-evidence-surface render --in <surface.json> --mode chords|spiral --out <render.json>\n");
	print("mv-evidence-surface render-leds240 --surface <surface.json> --out <frames.ndjson> [--frames 12] [--pointer-trace <wave27.ndjson>]\n");
	print("mv-evidence-surface verify-leds240 --surface <surface.json> --in <frames.ndjson> [--pointer-trace <wave27.ndjson>]\n");
	print("mv-evidence-surface render-leds240-esp32 --surface <surface.json> --out <emitter.ndjson> [--frames 12] [--frame-ms 50] [--pointer-trace <wave27.ndjson>]\n");
	print("mv-evidence-surface verify-leds240-esp32 --surface <surface.json> --in <emitter.ndjson> [--frame-ms 50] [--pointer-trace <wave27.ndjson>]\n");
	print("mv-evidence-surface emit-esp32-uart --surface <surface.json> --emitter <emitter.ndjson> --out <packets.ndjson> [--out-bin <packets.bin>] [--uart-crc none|crc8-xor-v0]\n");
	print("mv-evidence-surface verify-esp32-uart --surface <surface.json> --emitter <emitter.ndjson> --in <packets.ndjson> [--in-bin <packets.bin>] [--uart-crc none|crc8-xor-v0]\n");
	print("mv-evidence-surface decode-esp32-uart --surface <surface.json> --emitter <emitter.ndjson> --in-bin <packets.bin> --out <packets.ndjson> [--uart-crc none|crc8-xor-v0]\n");
	print("mv-evidence-surface wave31-verify --surface <surface.json> --emitter <emitter.ndjson> --in-bin <packets.bin> --receipt-out <decode-receipt.json> --verify-out <frame-verify.json> [--uart-crc none|crc8-xor-v0]\n");
}

static void
crcmode(void)
{
	if(strcmp(crc, "none") != 0 && strcmp(crc, CRC_ID) != 0) die("uart crc mode unknown");
}

static J*
emitterrows(Surf *s, int lead)	/* the emitter frames, each checked; lead: the message's lead-in */
{
	J *rows, *f;
	int i;

	rows = readnd(emitterf);
	for(i = 0; i < rows->nv; i++){
		f = rows->v[i];
		validateemitter(f);
		if(!trust && !same(get(f, "seed"), seedof(s->j))) die(smprint("emitter frame %d seed mismatch", i));
		if(toint(get(f, "t"), 0, 1000000000, smprint("emitter_frame[%d].t", i)) != i) die(smprint("emitter frame %d t sequence mismatch", i));
	}
	USED(lead);
	return rows;
}

static void
decode(int wave31)
{
	Surf s;
	J *rows, *f, *out, *dec, *rec, *rc, *vf, *ptr;
	Buf raw;
	int i, pb, k, c, n;
	uchar *ch, b[PKT + 1], p;
	long t, ms;
	char *h, *hh;

	crcmode();
	s = readsurface(surface);
	rows = readnd(emitterf);
	raw = readbin(inbin);
	pb = PKT + (strcmp(crc, CRC_ID) == 0);
	if(raw.n % pb != 0) die("uart packet framing mismatch");
	if(raw.n / pb != rows->nv) die("uart packet count mismatch");
	out = newarr();
	dec = newarr();
	for(i = 0; i < rows->nv; i++){
		f = rows->v[i];
		validateemitter(f);
		if(!trust && !same(get(f, "seed"), seedof(s.j))) die(smprint("emitter frame %d seed mismatch", i));
		if(toint(get(f, "t"), 0, 1000000000, smprint("emitter_frame[%d].t", i)) != i) die(smprint("emitter frame %d t sequence mismatch", i));
		ch = raw.b + i * pb;
		if(ch[0] != 0x30) die(smprint("uart packet %d version byte mismatch", i));
		if(ch[1] != 0x01) die(smprint("uart packet %d profile byte mismatch", i));
		t = ch[2] << 8 | ch[3];
		ms = ch[4] << 8 | ch[5];
		p = ch[6];
		if(p != 0xff && p >= RING) die(smprint("uart packet %d pointer out of range", i));
		if(pb > PKT){
			for(k = c = 0; k < PKT; k++)
				c ^= ch[k];
			if(ch[PKT] != c) die(smprint("uart packet %d crc mismatch", i));
		}
		if(t != i) die(smprint("uart packet %d t mismatch", i));
		if(!same(nstr(ms), get(f, "frame_ms"))) die(smprint("uart packet %d frame_ms mismatch", i));
		ptr = get(f, "pointer");
		if(p != (ptr->nv == 1 ? (long)number(ptr->v[0]) : 0xff)) die(smprint("uart packet %d pointer mismatch", i));
		rec = record(&s, f, crc);
		h = hex(ch, pb);
		hh = sval(get(rec, "packet_hex"));
		if(strcmp(h, hh) != 0) die(smprint("uart packet %d payload mismatch", i));
		push(out, rec);
		if(wave31){
			/* the frame the packet decodes to */
			J *d = mk(Jobj);
			J *pt = newarr();
			if(p != 0xff) push(pt, nstr(p));
			set(d, "authority", mkstr("advisory"));
			set(d, "dim", unmask(ch + 37));
			set(d, "frame_ms", nstr(ms));
			set(d, "mode", mkstr("leds240"));
			set(d, "on", unmask(ch + 7));
			set(d, "pointer", pt);
			set(d, "profile", mkstr("esp32.v0"));
			set(d, "ring_size", mkstr("240"));
			set(d, "seed", seedof(s.j));
			set(d, "t", nstr(t));
			set(d, "v", mkstr(EMITTER_V));
			if(!same(d, f)) die(smprint("wave31 frame verify mismatch at t=%d", i));
			push(dec, d);
		}
	}
	USED(b);
	n = out->nv;
	if(!wave31){
		writend(outf, out);
		print("ok mv-evidence-surface decode-esp32-uart packets=%d\n", n);
		return;
	}
	rc = mk(Jobj);
	set(rc, "authority", mkstr("advisory"));
	set(rc, "decode_ok", mkstr("1"));
	set(rc, "decode_profile_id", mkstr("wave31.decode_profile.esp32_uart.v0"));
	set(rc, "error_count", mkstr("0"));
	set(rc, "first_error_code", mkstr("none"));
	set(rc, "packet_count", nstr(n));
	set(rc, "packet_stream", out);
	set(rc, "seed", seedof(s.j));
	set(rc, "uart_crc", mkstr(crc));
	set(rc, "v", mkstr("wave31.hardware_decode_receipt.v0"));
	vf = mk(Jobj);
	set(vf, "authority", mkstr("advisory"));
	set(vf, "first_mismatch_t", mkstr("none"));
	set(vf, "frame_count", nstr(dec->nv));
	set(vf, "frame_stream", dec);
	set(vf, "frame_type", mkstr(EMITTER_V));
	set(vf, "frame_verify_id", mkstr("wave31.frame_verify.leds240.v0"));
	set(vf, "mismatch_count", mkstr("0"));
	set(vf, "seed", seedof(s.j));
	set(vf, "v", mkstr("wave31.frame_verify_result.v0"));
	set(vf, "verify_ok", mkstr("1"));
	writejson(receiptout, rc);
	writejson(verifyout, vf);
	print("ok mv-evidence-surface wave31-verify packets=%d frames=%d\n", n, dec->nv);
}

void
main(int argc, char *argv[])
{
	Surf s;
	J *x, *r, *c, *sp, *rows, *f, *g, *pk;
	long nf, ms, i;
	int fd, k, help;
	uchar b[PKT + 1];
	char *bin;
	long nbin;
	Buf raw;

	setfcr(getfcr() & ~(FPOVFL|FPZDIV|FPINVAL));
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		trust = 1;
		argc--;
		argv++;
	}
	help = 0;
	for(k = 1; k < argc; k++){	/* parseArgs: a flag takes the next when it is not empty */
		char *a = argv[k], *v = k + 1 < argc && argv[k+1][0] ? argv[k+1] : nil;
		static char *modes[] = { "build", "verify", "render", "render-leds240", "verify-leds240", "render-leds240-esp32", "verify-leds240-esp32", "emit-esp32-uart", "verify-esp32-uart", "decode-esp32-uart", "wave31-verify", nil };
		static char *flag[] = { "--p0", "--d", "--k-max", "--in", "--surface", "--pointer-trace", "--emitter", "--frames", "--frame-ms", "--uart-crc", "--out", "--out-bin", "--in-bin", "--receipt-out", "--verify-out", "--mode", nil };
		char **dst[] = { &seedarg[0], &seedarg[1], &seedarg[2], &inf, &surface, &trace, &emitterf, &frames, &framems, &crc, &outf, &outbin, &inbin, &receiptout, &verifyout, &rmode };
		int m;

		for(m = 0; modes[m] && strcmp(modes[m], a) != 0; m++)
			;
		if(modes[m]){ mode = modes[m]; continue; }
		for(m = 0; flag[m] && !(strcmp(flag[m], a) == 0 && v); m++)
			;
		if(flag[m]){ *dst[m] = v; k++; continue; }
		if(strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0){ help = 1; continue; }
		die(smprint("unknown arg: %s", a));
	}
	if(help || mode == nil){
		usage();
		exits(nil);
	}
	if(strcmp(mode, "build") == 0){
		if(!seedarg[0] || !seedarg[1] || !seedarg[2] || !outf) die("build requires --p0 --d --k-max --out");
		x = mk(Jobj);
		set(x, "authority", mkstr("advisory"));
		set(x, "d", mkstr(seedarg[1]));
		set(x, "k_max", mkstr(seedarg[2]));
		set(x, "p0", mkstr(seedarg[0]));
		set(x, "ring_size", mkstr("240"));
		set(x, "v", mkstr("wave30.evidence_surface.chords.v0"));
		params(x, &s);
		set(x, "chords", chords(s.p0, s.d, s.k));
		validatesurface(x);
		writejson(outf, x);
		print("ok mv-evidence-surface build\n");
	}else if(strcmp(mode, "verify") == 0){
		if(!inf) die("verify requires --in");
		readsurface(inf);
		print("ok mv-evidence-surface verify\n");
	}else if(strcmp(mode, "render") == 0){
		if(!inf || !outf) die("render requires --in --out");
		s = readsurface(inf);
		c = chords(s.p0, s.d, s.k);
		r = mk(Jobj);
		set(r, "authority", mkstr("advisory"));
		if(strcmp(rmode, "chords") == 0){
			set(r, "chords", c);
		}else if(strcmp(rmode, "spiral") == 0){
			sp = newarr();
			for(i = 0; i < c->nv; i++){
				g = mk(Jobj);
				set(g, "index", nstr(i + 1));
				set(g, "a", get(c->v[i], "lo"));
				set(g, "b", get(c->v[i], "hi"));
				set(g, "phase", nstr((i * s.d) % RING));
				push(sp, g);
			}
			set(r, "spiral", sp);
		}else
			die("render mode must be chords|spiral");
		set(r, "mode", mkstr(rmode));
		set(r, "ring_size", mkstr("240"));
		set(r, "seed", seedof(s.j));
		writejson(outf, r);
		print("ok mv-evidence-surface render mode=%s\n", rmode);
	}else if(strcmp(mode, "render-leds240") == 0 || strcmp(mode, "render-leds240-esp32") == 0){
		int esp = strcmp(mode, "render-leds240-esp32") == 0;
		if(!surface || !outf) die(smprint("%s requires --surface --out", mode));
		s = readsurface(surface);
		nf = toint(mkstr(frames), 1, 10000, "frames");
		ms = esp ? toint(mkstr(framems), 1, 5000, "frame_ms") : 0;
		if(trace) readsched(trace);
		rows = newarr();
		for(i = 0; i < nf; i++)
			push(rows, esp ? emitter(&s, i, ms) : frame(&s, i));
		writend(outf, rows);
		if(esp)
			print("ok mv-evidence-surface render-leds240-esp32 frames=%ld frame_ms=%ld\n", nf, ms);
		else
			print("ok mv-evidence-surface render-leds240 frames=%ld\n", nf);
	}else if(strcmp(mode, "verify-leds240") == 0 || strcmp(mode, "verify-leds240-esp32") == 0){
		int esp = strcmp(mode, "verify-leds240-esp32") == 0;
		char *what = esp ? "emitter frame" : "frame";
		if(!surface || !inf) die(smprint("%s requires --surface --in", mode));
		s = readsurface(surface);
		ms = esp ? toint(mkstr(framems), 1, 5000, "frame_ms") : 0;
		if(trace) readsched(trace);
		rows = readnd(inf);
		for(i = 0; i < rows->nv; i++){
			f = rows->v[i];
			if(esp) validateemitter(f); else validateframe(f);
			if(!trust && !same(get(f, "seed"), seedof(s.j))) die(smprint("%s %ld seed mismatch", what, i));
			if(toint(get(f, "t"), 0, 1000000000, smprint(esp ? "emitter_frame[%ld].t" : "frame[%ld].t", i)) != i) die(smprint("%s %ld t sequence mismatch", what, i));
			if(esp && !same(get(f, "frame_ms"), nstr(ms))) die(smprint("emitter frame %ld frame_ms mismatch", i));
			if(!trust && !same(f, esp ? emitter(&s, i, ms) : frame(&s, i))) die(smprint("%s %ld recompute mismatch", what, i));
		}
		if(esp)
			print("ok mv-evidence-surface verify-leds240-esp32 frames=%d frame_ms=%ld\n", rows->nv, ms);
		else
			print("ok mv-evidence-surface verify-leds240 frames=%d\n", rows->nv);
	}else if(strcmp(mode, "emit-esp32-uart") == 0){
		if(!surface || !emitterf || !outf) die("emit-esp32-uart requires --surface --emitter --out");
		s = readsurface(surface);
		rows = readnd(emitterf);
		crcmode();
		pk = newarr();
		for(i = 0; i < rows->nv; i++){
			f = rows->v[i];
			validateemitter(f);
			if(!trust && !same(get(f, "seed"), seedof(s.j))) die(smprint("emitter frame %ld seed mismatch", i));
			if(toint(get(f, "t"), 0, 1000000000, smprint("emitter_frame[%ld].t", i)) != i) die(smprint("emitter frame %ld t sequence mismatch", i));
			push(pk, record(&s, f, crc));
		}
		writend(outf, pk);
		if(outbin){
			if((fd = create(outbin, OWRITE, 0666)) < 0) die(smprint("cannot create %s", outbin));
			for(i = 0; i < pk->nv; i++){
				k = pack(get(pk->v[i], "frame"), strcmp(crc, CRC_ID) == 0, b);
				write(fd, b, k);
			}
			close(fd);
		}
		print("ok mv-evidence-surface emit-esp32-uart frames=%d\n", pk->nv);
	}else if(strcmp(mode, "verify-esp32-uart") == 0){
		if(!surface || !emitterf || !inf) die("verify-esp32-uart requires --surface --emitter --in");
		s = readsurface(surface);
		rows = readnd(emitterf);
		pk = readnd(inf);
		if(rows->nv != pk->nv) die("uart packet count mismatch");
		crcmode();
		bin = nil;
		nbin = 0;
		for(i = 0; i < rows->nv; i++){
			f = rows->v[i];
			validateemitter(f);
			if(!trust && !same(get(f, "seed"), seedof(s.j))) die(smprint("emitter frame %ld seed mismatch", i));
			if(toint(get(f, "t"), 0, 1000000000, smprint("emitter_frame[%ld].t", i)) != i) die(smprint("emitter frame %ld t sequence mismatch", i));
			g = pk->v[i];
			validaterecord(g);
			if(!trust && !same(get(g, "seed"), seedof(s.j))) die(smprint("uart packet %ld seed mismatch", i));
			if(!trust && !same(get(g, "frame"), f)) die(smprint("uart packet %ld frame mismatch", i));
			if(!eqs(get(g, "uart_crc"), crc)) die(smprint("uart packet %ld uart_crc mismatch", i));
			if(!trust && !same(g, record(&s, f, crc))) die(smprint("uart packet %ld recompute mismatch", i));
			k = get(g, "packet_hex")->ns / 2;
			bin = realloc(bin, nbin + k);
			unhex(get(g, "packet_hex"), (uchar*)bin + nbin);
			nbin += k;
		}
		if(inbin){
			raw = readbin(inbin);
			if(raw.n != nbin) die("uart bin length mismatch");
			if(memcmp(raw.b, bin, nbin) != 0) die("uart bin content mismatch");
		}
		print("ok mv-evidence-surface verify-esp32-uart packets=%d\n", pk->nv);
	}else if(strcmp(mode, "decode-esp32-uart") == 0){
		if(!surface || !emitterf || !inbin || !outf) die("decode-esp32-uart requires --surface --emitter --in-bin --out");
		decode(0);
	}else if(strcmp(mode, "wave31-verify") == 0){
		if(!surface || !emitterf || !inbin || !receiptout || !verifyout) die("wave31-verify requires --surface --emitter --in-bin --receipt-out --verify-out");
		decode(1);
	}
	USED(emitterrows);
	exits(nil);
}
