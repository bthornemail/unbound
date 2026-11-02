/*
 * zmvwir - Metaverse Kit's mv-runtime-handoff in Plan 9 C, literal: the wave30 surface's frames (and, if given,
 * its evidence bundle, emitter frames, UART packets and wave31 decode receipt and frame verify) turned into a
 * world.ir.v0 (an LED entity for each index the frames touch, with its hits and its last state; a frame event
 * for each frame; the inputs attached to portal:wave30), or the wave31 pair alone into one, and a world IR's
 * shape verified. The tool reads and names these by digest; here they are the literals of zmvsurf and zmvevid
 * (a surface's seed {d, k_max, p0}, a packet holding its frame, a receipt and a verify holding their streams, a
 * bundle holding its subject and evidence), compared as literals where the tool compares digests, and the
 * world IR holds them: an LED's seed, a frame event's frame, an attachment's ref (its kind without "_digest"),
 * a wave31 component's seed and stream, an event's receipt or verify. Two of the tool's readings are kept as
 * they are: a packet's hex must be 134 digits, so a crc8 stream (136) is refused, and a bundle's evidence must
 * ascend by v|... in code units, where mv-evidence-bundle orders v by localeCompare.
 *	zmvwir build-world-ir-wave30|build-world-ir --surface F --frames F --out F [--bundle F] [--emitter F]
 *		[--uart F] [--wave31-receipt F --wave31-frame-verify F] [--world ID]
 *	zmvwir build-world-ir-wave31 --receipt F --frame-verify F --out F [--world ID]
 *	zmvwir verify-world-ir --in F
 *	zmvwir -n ...		the falsifier: the inputs trusted as written (no seed, frame or mapping compared)
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


static char*
sval(J *v)
{
	return utf(v->s, v->ns);
}

static int
isobj(J *x)
{
	return x != nil && x->t == Jobj;
}

static long
tointstr(J *s, long min, long max, char *label)	/* toIntString: /^\d+$/, an integer in range */
{
	double n;

	if(!digits(s, 0)) die(smprint("%s must be decimal string", label));
	n = number(s);
	if(n != floor(n) || n < min || n > max) die(smprint("%s out of range", label));
	return (long)n;
}

static int
inarr(J *a, J *x)
{
	int i;

	for(i = 0; i < a->nv; i++)
		if(same(a->v[i], x))
			return 1;
	return 0;
}

static void
idxarray(J *a, char *label)	/* validateIdxArray */
{
	J *seen;
	long n, prev;
	int i;

	if(a == nil || a->t != Jarr) die(smprint("%s must be array", label));
	prev = -1;
	seen = mk(Jarr);
	for(i = 0; i < a->nv; i++){
		n = tointstr(a->v[i], 0, 239, smprint("%s[]", label));
		if(inarr(seen, a->v[i])) die(smprint("%s duplicate index", label));
		if(n < prev) die(smprint("%s not sorted", label));
		push(seen, a->v[i]);
		prev = n;
	}
}

static J*
seedof(J *s)
{
	J *x;

	x = mk(Jobj);
	set(x, "d", get(s, "d"));
	set(x, "k_max", get(s, "k_max"));
	set(x, "p0", get(s, "p0"));
	return x;
}

static J*
jarr(void)
{
	return mk(Jarr);
}

static J*
lstr(long n)
{
	return mkstr(smprint("%ld", n));
}

static J*
readnd(char *f)	/* readNdjson: the lines, trimmed, the empty ones dropped, each JSON */
{
	int fd;
	long n, len;
	char *s, buf[8192], *p, *nl, *e;
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
	rows = jarr();
	for(p = s; p; p = nl ? nl + 1 : nil){
		nl = strchr(p, '\n');
		if(nl) *nl = 0;
		while(*p && strchr(" \t\r\v\f", *p)) p++;
		for(e = p + strlen(p); e > p && strchr(" \t\r\v\f", e[-1]); e--)
			;
		*e = 0;
		if(*p == 0)
			continue;
		in = ip = p;
		push(rows, value());
		ws();
		if(*ip) bad("Unexpected non-whitespace character after JSON");
	}
	if(rows->nv == 0) die("frames NDJSON empty");
	return rows;
}

/* ── the inputs ── */

static char *SURFKEYS[] = { "v", "authority", "chords", "d", "k_max", "p0", "ring_size", nil };
static char *FRAMEKEYS[] = { "v", "authority", "t", "ring_size", "mode", "pointer_on", "chord_on", "chord_dim", "seed", nil };
static char *BUNDLEKEYS[] = { "v", "authority", "subject", "claim_type", "evidence", nil };
static char *EMITKEYS[] = { "v", "authority", "t", "mode", "profile", "ring_size", "frame_ms", "on", "dim", "pointer", "seed", nil };
static char *UARTKEYS[] = { "v", "authority", "t", "profile", "uart_crc", "packet_bytes", "packet_hex", "frame", "seed", nil };
static char *RECEIPTKEYS[] = { "v", "authority", "decode_profile_id", "seed", "packet_stream", "uart_crc", "packet_count", "decode_ok", "error_count", "first_error_code", nil };
static char *VERIFYKEYS[] = { "v", "authority", "frame_verify_id", "frame_type", "seed", "frame_stream", "frame_count", "verify_ok", "mismatch_count", "first_mismatch_t", nil };

static void
validatesurface(J *s)
{
	if(!isobj(s)) die("surface must be object");
	keyset(s, SURFKEYS, "surface");
	if(!eqs(get(s, "v"), "wave30.evidence_surface.chords.v0")) die("surface version mismatch");
	if(!eqs(get(s, "authority"), "advisory")) die("surface authority must be advisory");
	if(!eqs(get(s, "ring_size"), "240")) die("surface ring_size mismatch");
}

static void
validateframe(J *f, int i, J *seed)
{
	J *on, *dim;
	int k;

	if(!isobj(f)) die(smprint("frame[%d] must be object", i));
	keyset(f, FRAMEKEYS, smprint("frame[%d]", i));
	if(!eqs(get(f, "v"), "wave30.evidence_surface_frame.v0")) die(smprint("frame[%d] version mismatch", i));
	if(!eqs(get(f, "authority"), "advisory")) die(smprint("frame[%d] authority must be advisory", i));
	if(!eqs(get(f, "ring_size"), "240")) die(smprint("frame[%d] ring_size mismatch", i));
	if(!eqs(get(f, "mode"), "leds240")) die(smprint("frame[%d] mode mismatch", i));
	if(!trust && !same(get(f, "seed"), seed)) die(smprint("frame[%d] seed mismatch", i));
	if(tointstr(get(f, "t"), 0, 1000000000, smprint("frame[%d].t", i)) != i) die(smprint("frame[%d] t sequence mismatch", i));
	idxarray(get(f, "pointer_on"), smprint("frame[%d].pointer_on", i));
	idxarray(on = get(f, "chord_on"), smprint("frame[%d].chord_on", i));
	idxarray(dim = get(f, "chord_dim"), smprint("frame[%d].chord_dim", i));
	for(k = 0; k < dim->nv; k++)
		if(inarr(on, dim->v[k])) die(smprint("frame[%d] chord_on intersects chord_dim", i));
}

static void
validatebundle(J *b)
{
	J *ev, *e, *c, *v, *seen;
	char *k, *prev;
	int i;

	if(!isobj(b)) die("bundle must be object");
	keyset(b, BUNDLEKEYS, "bundle");
	if(!eqs(get(b, "v"), "wave30.evidence_bundle.v0")) die("bundle version mismatch");
	if(!eqs(get(b, "authority"), "advisory")) die("bundle authority must be advisory");
	c = get(b, "claim_type");
	if(c->t != Jstr || c->ns == 0) die("bundle.claim_type invalid");
	ev = get(b, "evidence");
	if(ev->t != Jarr || ev->nv == 0) die("bundle.evidence must be non-empty array");
	prev = "";
	seen = jarr();
	for(i = 0; i < ev->nv; i++){
		e = ev->v[i];
		if(!isobj(e)) die(smprint("bundle.evidence[%d] must be object", i));
		v = get(e, "v");
		if(v == nil || v->t != Jstr || v->ns == 0) die(smprint("bundle.evidence[%d].v invalid", i));
		k = smprint("%s|%s", sval(v), canon(e));	/* v|the literal, where the tool has v|digest */
		if(inarr(seen, mkstr(k))) die("bundle.evidence duplicate entry");
		push(seen, mkstr(k));
		if(*prev && cmp16(k, prev) < 0) die("bundle.evidence ordering mismatch");
		prev = k;
	}
}

static void
validateemitter(J *f, int i, J *seed)
{
	J *on, *dim, *p;
	int k;

	if(!isobj(f)) die(smprint("emitter[%d] must be object", i));
	keyset(f, EMITKEYS, smprint("emitter[%d]", i));
	if(!eqs(get(f, "v"), "wave30.evidence_surface_emitter_frame.esp32.v0")) die(smprint("emitter[%d] version mismatch", i));
	if(!eqs(get(f, "authority"), "advisory")) die(smprint("emitter[%d] authority must be advisory", i));
	if(!eqs(get(f, "mode"), "leds240")) die(smprint("emitter[%d] mode mismatch", i));
	if(!eqs(get(f, "profile"), "esp32.v0")) die(smprint("emitter[%d] profile mismatch", i));
	if(!eqs(get(f, "ring_size"), "240")) die(smprint("emitter[%d] ring_size mismatch", i));
	tointstr(get(f, "frame_ms"), 1, 5000, smprint("emitter[%d].frame_ms", i));
	if(!trust && !same(get(f, "seed"), seed)) die(smprint("emitter[%d] seed mismatch", i));
	if(tointstr(get(f, "t"), 0, 1000000000, smprint("emitter[%d].t", i)) != i) die(smprint("emitter[%d] t sequence mismatch", i));
	idxarray(on = get(f, "on"), smprint("emitter[%d].on", i));
	idxarray(dim = get(f, "dim"), smprint("emitter[%d].dim", i));
	idxarray(p = get(f, "pointer"), smprint("emitter[%d].pointer", i));
	if(p->nv > 1) die(smprint("emitter[%d] pointer length must be 0..1", i));
	for(k = 0; k < dim->nv; k++)
		if(inarr(on, dim->v[k])) die(smprint("emitter[%d] on intersects dim", i));
	for(k = 0; k < p->nv; k++)
		if(!inarr(on, p->v[k])) die(smprint("emitter[%d] pointer must be subset of on", i));
}

static J*
bynumber(J *a)	/* .sort((a, b) => Number(a) - Number(b)), stable */
{
	J *s, *t;
	int i, m;

	s = jarr();
	for(i = 0; i < a->nv; i++)
		push(s, a->v[i]);
	for(i = 1; i < s->nv; i++)
		for(m = i; m > 0 && number(s->v[m]) < number(s->v[m-1]); m--){
			t = s->v[m]; s->v[m] = s->v[m-1]; s->v[m-1] = t;
		}
	return s;
}

static void
againstframes(J *frames, J *em)	/* validateEmitterAgainstFrames */
{
	J *fr, *e, *u, *x, *dim;
	int i, k;

	if(em->nv != frames->nv) die("emitter/frame length mismatch");
	if(trust)
		return;
	for(i = 0; i < frames->nv; i++){
		fr = frames->v[i];
		e = em->v[i];
		u = jarr();
		x = get(fr, "pointer_on");
		for(k = 0; k < x->nv; k++) if(!inarr(u, x->v[k])) push(u, x->v[k]);
		x = get(fr, "chord_on");
		for(k = 0; k < x->nv; k++) if(!inarr(u, x->v[k])) push(u, x->v[k]);
		u = bynumber(u);
		dim = jarr();
		x = get(fr, "chord_dim");
		for(k = 0; k < x->nv; k++) if(!inarr(u, x->v[k])) push(dim, x->v[k]);
		dim = bynumber(dim);
		if(!same(get(e, "on"), u)) die(smprint("emitter[%d] mapping mismatch for on", i));
		if(!same(get(e, "dim"), dim)) die(smprint("emitter[%d] mapping mismatch for dim", i));
		if(!same(get(e, "pointer"), get(fr, "pointer_on"))) die(smprint("emitter[%d] mapping mismatch for pointer", i));
	}
}

static int
hex134(J *h)	/* /^[0-9a-f]+$/ and 134 digits: 67 bytes, no crc */
{
	int i;

	if(h == nil || h->t != Jstr || h->ns != 134) return 0;
	for(i = 0; i < h->ns; i++)
		if(!((h->s[i] >= '0' && h->s[i] <= '9') || (h->s[i] >= 'a' && h->s[i] <= 'f')))
			return 0;
	return 1;
}

static void
validateuart(J *p, int i, J *seed, J *frame)
{
	if(!isobj(p)) die(smprint("uart[%d] must be object", i));
	keyset(p, UARTKEYS, smprint("uart[%d]", i));
	if(!eqs(get(p, "v"), "wave30.evidence_surface_uart_packet.esp32.v0")) die(smprint("uart[%d] version mismatch", i));
	if(!eqs(get(p, "authority"), "advisory")) die(smprint("uart[%d] authority must be advisory", i));
	if(!eqs(get(p, "profile"), "esp32.uart.v0")) die(smprint("uart[%d] profile mismatch", i));
	if(!eqs(get(p, "uart_crc"), "none") && !eqs(get(p, "uart_crc"), "crc8-xor-v0")) die(smprint("uart[%d] uart_crc invalid", i));
	if(!eqs(get(p, "packet_bytes"), "67") && !eqs(get(p, "packet_bytes"), "68")) die(smprint("uart[%d] packet_bytes invalid", i));
	if(!hex134(get(p, "packet_hex"))) die(smprint("uart[%d] packet_hex invalid", i));
	if(!trust && !same(get(p, "seed"), seed)) die(smprint("uart[%d] seed mismatch", i));
	if(!trust && !same(get(p, "frame"), frame)) die(smprint("uart[%d] frame mismatch", i));
	if(tointstr(get(p, "t"), 0, 1000000000, smprint("uart[%d].t", i)) != i) die(smprint("uart[%d] t sequence mismatch", i));
}

static void
bit(J *x, char *label)
{
	if(!eqs(x, "0") && !eqs(x, "1")) die(smprint("%s must be 0 or 1", label));
}

static void
validatereceipt(J *x)
{
	long err;

	if(!isobj(x)) die("wave31 receipt must be object");
	keyset(x, RECEIPTKEYS, "wave31 receipt");
	if(!eqs(get(x, "v"), "wave31.hardware_decode_receipt.v0")) die("wave31 receipt version mismatch");
	if(!eqs(get(x, "authority"), "advisory")) die("wave31 receipt authority must be advisory");
	if(!eqs(get(x, "decode_profile_id"), "wave31.decode_profile.esp32_uart.v0")) die("wave31 receipt decode_profile_id mismatch");
	if(!eqs(get(x, "uart_crc"), "none") && !eqs(get(x, "uart_crc"), "crc8-xor-v0")) die("wave31 receipt uart_crc unknown");
	tointstr(get(x, "packet_count"), 0, 1000000000, "wave31 receipt packet_count");
	bit(get(x, "decode_ok"), "wave31 receipt decode_ok");
	err = tointstr(get(x, "error_count"), 0, 1000000000, "wave31 receipt error_count");
	if(eqs(get(x, "decode_ok"), "1")){
		if(err != 0) die("wave31 receipt decode_ok=1 requires error_count=0");
		if(!eqs(get(x, "first_error_code"), "none")) die("wave31 receipt decode_ok=1 requires first_error_code=none");
	}
}

static void
validateverify(J *x)
{
	long mm;

	if(!isobj(x)) die("wave31 frame verify must be object");
	keyset(x, VERIFYKEYS, "wave31 frame verify");
	if(!eqs(get(x, "v"), "wave31.frame_verify_result.v0")) die("wave31 frame verify version mismatch");
	if(!eqs(get(x, "authority"), "advisory")) die("wave31 frame verify authority must be advisory");
	if(!eqs(get(x, "frame_verify_id"), "wave31.frame_verify.leds240.v0")) die("wave31 frame verify frame_verify_id mismatch");
	if(!eqs(get(x, "frame_type"), "wave30.evidence_surface_emitter_frame.esp32.v0")) die("wave31 frame verify frame_type mismatch");
	tointstr(get(x, "frame_count"), 0, 1000000000, "wave31 frame verify frame_count");
	bit(get(x, "verify_ok"), "wave31 frame verify verify_ok");
	mm = tointstr(get(x, "mismatch_count"), 0, 1000000000, "wave31 frame verify mismatch_count");
	if(eqs(get(x, "verify_ok"), "1")){
		if(mm != 0) die("wave31 frame verify verify_ok=1 requires mismatch_count=0");
		if(!eqs(get(x, "first_mismatch_t"), "none")) die("wave31 frame verify verify_ok=1 requires first_mismatch_t=none");
	}else
		tointstr(get(x, "first_mismatch_t"), 0, 1000000000, "wave31 frame verify first_mismatch_t");
}

/* ── the world IR ── */

static void
validateworld(J *ir)
{
	static char *IRKEYS[] = { "world", "entities", "zones", "rules", "portals", "attachments", "events", nil };
	static char *ENTKEYS[] = { "id", "components", "owner", "zone", nil };
	static char *ARRS[] = { "zones", "rules", "portals", "attachments", "events", nil };
	J *es, *e, *ids, *c, *cs, *x, *t, *d, *zs, *z, *evs, *ev, *a;
	int i, j;

	if(!isobj(ir)) die("world.ir must be object");
	keyset(ir, IRKEYS, "world.ir");
	x = get(ir, "world");
	if(x->t != Jstr || x->ns == 0) die("world.ir.world must be non-empty string");
	es = get(ir, "entities");
	if(es->t != Jarr) die("world.ir.entities must be array");
	ids = jarr();
	for(i = 0; i < es->nv; i++){
		e = es->v[i];
		if(!isobj(e)) die(smprint("entity[%d] must be object", i));
		keyset(e, ENTKEYS, smprint("entity[%d]", i));
		x = get(e, "id");
		if(x->t != Jstr || x->ns == 0) die(smprint("entity[%d].id invalid", i));
		if(inarr(ids, x)) die(smprint("entity[%d].id duplicate", i));
		push(ids, x);
		cs = get(e, "components");
		if(cs->t != Jarr) die(smprint("entity[%d].components must be array", i));
		x = get(e, "owner");
		if(x->t != Jstr || x->ns == 0) die(smprint("entity[%d].owner invalid", i));
		x = get(e, "zone");
		if(x->t != Jstr || x->ns == 0) die(smprint("entity[%d].zone invalid", i));
		for(j = 0; j < cs->nv; j++){
			c = cs->v[j];
			if(!isobj(c)) die(smprint("entity[%d].components[%d] must be object", i, j));
			t = get(c, "type");
			if(t == nil || t->t != Jstr || t->ns == 0) die(smprint("entity[%d].components[%d].type invalid", i, j));
			d = get(c, "data");
			if(d != nil && !isobj(d)) die(smprint("entity[%d].components[%d].data invalid", i, j));
		}
	}
	for(i = 0; ARRS[i]; i++)
		if(get(ir, ARRS[i])->t != Jarr) die(smprint("world.ir.%s must be array", ARRS[i]));
	zs = get(ir, "zones");
	for(i = 0; i < zs->nv; i++){
		z = zs->v[i];
		x = get(z, "id");
		if(!isobj(z) || x == nil || x->t != Jstr || x->ns == 0) die(smprint("zone[%d] invalid", i));
	}
	evs = get(ir, "events");
	for(i = 0; i < evs->nv; i++){
		ev = evs->v[i];
		if(!isobj(ev)) die(smprint("event[%d] must be object", i));
		x = get(ev, "id");
		if(x == nil || x->t != Jstr || x->ns == 0) die(smprint("event[%d].id invalid", i));
		if(!isobj(get(ev, "intent"))) die(smprint("event[%d].intent invalid", i));
		a = get(ev, "authority");
		if(a != nil && a->t != Jstr) die(smprint("event[%d].authority invalid", i));
	}
}

static J*
obj2(char *k1, J *v1, char *k2, J *v2)
{
	J *o;

	o = mk(Jobj);
	set(o, k1, v1);
	if(k2) set(o, k2, v2);
	return o;
}

static J*
strs(char *a, char *b, char *c)
{
	J *x;

	x = jarr();
	push(x, mkstr(a));
	push(x, mkstr(b));
	push(x, mkstr(c));
	return x;
}

static J*
portal(char *src)
{
	J *e, *p, *a;

	e = mk(Jobj);
	set(e, "source", mkstr(src));
	set(e, "mode", mkstr("projection_bundle"));
	set(e, "version", mkstr("world.ir.v0"));
	p = mk(Jobj);
	set(p, "id", mkstr(smprint("portal:%s", src)));
	set(p, "entry", e);
	a = jarr();
	push(a, p);
	return a;
}

static J*
attach(char *id, char *kind, J *ref)
{
	J *a;

	a = mk(Jobj);
	set(a, "id", mkstr(id));
	set(a, "target", mkstr("portal:wave30"));
	set(a, "kind", mkstr(kind));
	set(a, "ref", ref);
	return a;
}

typedef struct Led Led;
struct Led { J *idx; long on, dim, ptr; J *lon, *ldim, *lptr, *lt; };

static Led *leds;
static int nleds;

static Led*
led(J *idx)	/* ensure */
{
	int i;

	for(i = 0; i < nleds; i++)
		if(same(leds[i].idx, idx))
			return &leds[i];
	leds = realloc(leds, (nleds + 1) * sizeof(Led));
	memset(&leds[nleds], 0, sizeof(Led));
	leds[nleds].idx = idx;
	return &leds[nleds++];
}

static void
setlast(J *idx, char *on, char *dim, char *ptr, J *t)
{
	Led *l;

	l = led(idx);
	l->lon = mkstr(on);
	l->ldim = mkstr(dim);
	l->lptr = mkstr(ptr);
	l->lt = t;
}

static J*
wave30(J *surface, J *frames, char *world, J *bundle, J *uart, J *receipt, J *verify)
{
	J *fr, *on, *dim, *ptr, *x, *ents, *e, *cs, *evs, *in, *ev, *at, *ir, *seed;
	Led tmp;
	int i, k, m;

	seed = seedof(surface);
	for(i = 0; i < frames->nv; i++){
		fr = frames->v[i];
		on = get(fr, "chord_on");
		dim = get(fr, "chord_dim");
		ptr = get(fr, "pointer_on");
		for(k = 0; k < on->nv; k++){
			led(on->v[k])->on++;
			setlast(on->v[k], "1", inarr(dim, on->v[k]) ? "1" : "0", inarr(ptr, on->v[k]) ? "1" : "0", get(fr, "t"));
		}
		for(k = 0; k < dim->nv; k++){
			led(dim->v[k])->dim++;
			if(!inarr(on, dim->v[k]))
				setlast(dim->v[k], "0", "1", inarr(ptr, dim->v[k]) ? "1" : "0", get(fr, "t"));
		}
		for(k = 0; k < ptr->nv; k++){
			led(ptr->v[k])->ptr++;
			if(!inarr(on, ptr->v[k]) && !inarr(dim, ptr->v[k]))
				setlast(ptr->v[k], "0", "0", "1", get(fr, "t"));
		}
	}
	for(k = 1; k < nleds; k++)	/* by Number, stable */
		for(m = k; m > 0 && number(leds[m].idx) < number(leds[m-1].idx); m--){
			tmp = leds[m]; leds[m] = leds[m-1]; leds[m-1] = tmp;
		}
	ents = jarr();
	for(k = 0; k < nleds; k++){
		cs = jarr();
		push(cs, obj2("type", mkstr("wave30.led.index"), "data", obj2("index", leds[k].idx, nil, nil)));
		x = mk(Jobj);
		set(x, "on_hits", lstr(leds[k].on));
		set(x, "dim_hits", lstr(leds[k].dim));
		set(x, "pointer_hits", lstr(leds[k].ptr));
		set(x, "seed", seed);
		push(cs, obj2("type", mkstr("wave30.led.activity"), "data", x));
		x = mk(Jobj);
		set(x, "last_on", leds[k].lon);
		set(x, "last_dim", leds[k].ldim);
		set(x, "last_pointer", leds[k].lptr);
		set(x, "last_t", leds[k].lt);
		push(cs, obj2("type", mkstr("wave30.runtime.display_state"), "data", x));
		e = mk(Jobj);
		set(e, "id", mkstr(smprint("led:%s", sval(leds[k].idx))));
		set(e, "owner", mkstr("projection:wave30"));
		set(e, "zone", mkstr("ring:240"));
		set(e, "components", cs);
		push(ents, e);
	}
	evs = jarr();
	for(i = 0; i < frames->nv; i++){
		fr = frames->v[i];
		in = mk(Jobj);
		set(in, "type", mkstr("wave30.frame.leds240"));
		set(in, "t", get(fr, "t"));
		set(in, "pointer_on", get(fr, "pointer_on"));
		set(in, "chord_on", get(fr, "chord_on"));
		set(in, "chord_dim", get(fr, "chord_dim"));
		set(in, "seed", get(fr, "seed"));
		set(in, "frame", fr);
		ev = mk(Jobj);
		set(ev, "id", mkstr(smprint("frame:%s", sval(get(fr, "t")))));
		set(ev, "authority", mkstr("advisory"));
		set(ev, "intent", in);
		push(evs, ev);
	}
	at = jarr();	/* by id, as localeCompare orders these six */
	if(bundle) push(at, attach("evidence:bundle", "wave30.bundle", bundle));
	push(at, attach("evidence:frame_stream", "wave30.frame_stream", frames));
	if(uart) push(at, attach("evidence:packet_stream", "wave30.packet_stream", uart));
	push(at, attach("evidence:surface", "wave30.surface", surface));
	if(receipt) push(at, attach("verification:wave31:decode_receipt", "wave31.decode_receipt.advisory", receipt));
	if(verify) push(at, attach("verification:wave31:frame_verify", "wave31.frame_verify.advisory", verify));
	ir = mk(Jobj);
	set(ir, "world", mkstr(world));
	set(ir, "entities", ents);
	set(ir, "zones", jarr());
	push(get(ir, "zones"), obj2("id", mkstr("ring:240"), "tags", strs("wave30", "leds240", "projection")));
	set(ir, "rules", jarr());
	set(ir, "portals", portal("wave30"));
	set(ir, "attachments", at);
	set(ir, "events", evs);
	return ir;
}

static J*
datafrom(J *x, char **keys)	/* the listed fields, in order */
{
	J *d;
	int i;

	d = mk(Jobj);
	for(i = 0; keys[i]; i++)
		set(d, keys[i], get(x, keys[i]));
	return d;
}

static J*
wave31(J *rc, J *vf, char *world)
{
	static char *RK[] = { "decode_profile_id", "seed", "packet_stream", "uart_crc", "packet_count", "decode_ok", "error_count", "first_error_code", nil };
	static char *VK[] = { "frame_verify_id", "frame_type", "seed", "frame_stream", "frame_count", "verify_ok", "mismatch_count", "first_mismatch_t", nil };
	J *ents, *e, *cs, *evs, *ev, *in, *ir;

	ents = jarr();
	cs = jarr();
	push(cs, obj2("type", mkstr("wave31.decode.receipt"), "data", datafrom(rc, RK)));
	e = mk(Jobj);
	set(e, "id", mkstr("wave31:decode_receipt"));
	set(e, "owner", mkstr("projection:wave31"));
	set(e, "zone", mkstr("verification:wave31"));
	set(e, "components", cs);
	push(ents, e);
	cs = jarr();
	push(cs, obj2("type", mkstr("wave31.frame.verify"), "data", datafrom(vf, VK)));
	e = mk(Jobj);
	set(e, "id", mkstr("wave31:frame_verify_result"));
	set(e, "owner", mkstr("projection:wave31"));
	set(e, "zone", mkstr("verification:wave31"));
	set(e, "components", cs);
	push(ents, e);
	evs = jarr();
	in = mk(Jobj);
	set(in, "type", mkstr("wave31.hardware_decode_receipt.v0"));
	set(in, "receipt", rc);
	set(in, "decode_ok", get(rc, "decode_ok"));
	set(in, "packet_count", get(rc, "packet_count"));
	ev = mk(Jobj);
	set(ev, "id", mkstr("event:wave31:decode_receipt"));
	set(ev, "authority", mkstr("advisory"));
	set(ev, "intent", in);
	push(evs, ev);
	in = mk(Jobj);
	set(in, "type", mkstr("wave31.frame_verify_result.v0"));
	set(in, "verify", vf);
	set(in, "verify_ok", get(vf, "verify_ok"));
	set(in, "frame_count", get(vf, "frame_count"));
	ev = mk(Jobj);
	set(ev, "id", mkstr("event:wave31:frame_verify_result"));
	set(ev, "authority", mkstr("advisory"));
	set(ev, "intent", in);
	push(evs, ev);
	ir = mk(Jobj);
	set(ir, "world", mkstr(world));
	set(ir, "entities", ents);
	set(ir, "zones", jarr());
	push(get(ir, "zones"), obj2("id", mkstr("verification:wave31"), "tags", strs("wave31", "verification", "projection")));
	set(ir, "rules", jarr());
	set(ir, "portals", portal("wave31"));
	set(ir, "attachments", jarr());
	set(ir, "events", evs);
	return ir;
}

static void
usage(void)
{
	print("mv-runtime-handoff build-world-ir-wave30 --surface <surface.json> --frames <frames.ndjson> --out <world.ir.json> [--bundle <bundle.json>] [--emitter <emitter.ndjson>] [--uart <packets.ndjson>] [--wave31-receipt <receipt.json> --wave31-frame-verify <verify.json>] [--world <id>]\n");
	print("mv-runtime-handoff build-world-ir --surface <surface.json> --frames <frames.ndjson> --out <world.ir.json> [--world <id>]  # alias for build-world-ir-wave30\n");
	print("mv-runtime-handoff build-world-ir-wave31 --receipt <receipt.json> --frame-verify <verify.json> --out <world.ir.json> [--world <id>]\n");
	print("mv-runtime-handoff verify-world-ir --in <world.ir.json>\n");
}

void
main(int argc, char *argv[])
{
	static char *flag[] = { "--surface", "--frames", "--bundle", "--emitter", "--uart", "--receipt", "--frame-verify", "--wave31-receipt", "--wave31-frame-verify", "--out", "--in", "--world", nil };
	char *val[nelem(flag)], *mode, *a, *v, *world;
	J *surface, *frames, *bundle, *em, *uart, *rc, *vf, *ir, *seed;
	int i, m;
	enum { Surface, Frames, Bundle, Emitter, Uart, Receipt, FrameVerify, W31Receipt, W31Verify, Out, In, World };

	setfcr(getfcr() & ~(FPOVFL|FPZDIV|FPINVAL));
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		trust = 1;
		argc--;
		argv++;
	}
	memset(val, 0, sizeof val);
	mode = nil;
	for(i = 1; i < argc; i++){	/* parseArgs: a flag takes the next when it is not empty */
		a = argv[i];
		v = i + 1 < argc && argv[i+1][0] ? argv[i+1] : nil;
		if(strcmp(a, "build-world-ir-wave30") == 0 || strcmp(a, "build-world-ir") == 0) mode = "build-world-ir-wave30";
		else if(strcmp(a, "build-world-ir-wave31") == 0) mode = "build-world-ir-wave31";
		else if(strcmp(a, "verify-world-ir") == 0) mode = "verify-world-ir";
		else if(strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) mode = "help";
		else {
			for(m = 0; flag[m] && !(strcmp(flag[m], a) == 0 && v); m++)
				;
			if(flag[m] == nil) die(smprint("unknown arg: %s", a));
			val[m] = v;
			i++;
		}
	}
	if(mode == nil || strcmp(mode, "help") == 0){
		usage();
		exits(nil);
	}
	if(strcmp(mode, "build-world-ir-wave30") == 0){
		if(!val[Surface] || !val[Frames] || !val[Out]) die("build-world-ir-wave30 requires --surface --frames --out");
		world = val[World] ? val[World] : "wave30-surface-v0";
		surface = readjson(val[Surface]);
		validatesurface(surface);
		seed = seedof(surface);
		frames = readnd(val[Frames]);
		for(i = 0; i < frames->nv; i++)
			validateframe(frames->v[i], i, seed);
		bundle = em = uart = rc = vf = nil;
		if(val[Bundle]){
			bundle = readjson(val[Bundle]);
			validatebundle(bundle);
		}
		if(val[Emitter]){
			em = readnd(val[Emitter]);
			for(i = 0; i < em->nv; i++)
				validateemitter(em->v[i], i, seed);
			againstframes(frames, em);
		}
		if(val[Uart]){
			if(em == nil) die("--uart requires --emitter context");
			uart = readnd(val[Uart]);
			if(uart->nv != em->nv) die("uart/emitter length mismatch");
			for(i = 0; i < uart->nv; i++)
				validateuart(uart->v[i], i, seed, em->v[i]);
		}
		if(val[W31Receipt] || val[W31Verify]){
			if(!val[W31Receipt] || !val[W31Verify]) die("wave31 verification metadata requires both --wave31-receipt and --wave31-frame-verify");
			rc = readjson(val[W31Receipt]);
			vf = readjson(val[W31Verify]);
			validatereceipt(rc);
			validateverify(vf);
			if(!trust && !same(get(rc, "seed"), seed)) die("wave31 receipt seed mismatch vs wave30 surface");
			if(!trust && !same(get(vf, "seed"), seed)) die("wave31 frame verify seed mismatch vs wave30 surface");
		}
		ir = wave30(surface, frames, world, bundle, uart, rc, vf);
		validateworld(ir);
		writejson(val[Out], ir);
		print("ok mv-runtime-handoff build-world-ir-wave30 world=%s\n", world);
	}else if(strcmp(mode, "build-world-ir-wave31") == 0){
		if(!val[Receipt] || !val[FrameVerify] || !val[Out]) die("build-world-ir-wave31 requires --receipt --frame-verify --out");
		rc = readjson(val[Receipt]);
		vf = readjson(val[FrameVerify]);
		validatereceipt(rc);
		validateverify(vf);
		if(!trust && !same(get(rc, "seed"), get(vf, "seed"))) die("wave31 receipt/verify seed mismatch");
		world = val[World] ? val[World] : "wave31-verify-v0";
		ir = wave31(rc, vf, world);
		validateworld(ir);
		writejson(val[Out], ir);
		print("ok mv-runtime-handoff build-world-ir-wave31 world=%s\n", world);
	}else{
		if(!val[In]) die("verify-world-ir requires --in");
		ir = readjson(val[In]);
		validateworld(ir);
		print("ok mv-runtime-handoff verify-world-ir world=%s\n", sval(get(ir, "world")));
	}
	exits(nil);
}
