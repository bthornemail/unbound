/*
 * zmvuni - Metaverse Kit's mv-universe-ingest in Plan 9 C, literal: a payload built into a canonical package
 * (its bytes held), a package carried as an Aztec bundle (zlib, base64url, cut into chunks), and a package, a
 * bundle, a universe XML, an ASG or a media reference ingested into a scene; and readiness over five fixtures.
 * The tool names all of these by hash (sha3 or sha256: a payload digest, a package digest, a bundle id, chunk
 * and manifest digests, a scene's source and canonical digests, a package node by its digest); here there is no
 * hash (the author's choices, 2026-11-02): a package keeps its bytes and the payload's own claims as written; a
 * bundle and its chunks are named by the package's name, "package:" and its law_version; a scene's package node
 * is that name and a media node "media:" and its uri; a scene holds its source (the text read), its canonical
 * form (the package, the XML text, the ASG's nodes and edges, the media reference) and the path as given. A
 * bundle is compressed by libflate at level 9; its bytes are not zlib's, but each reads the other's.
 *	zmvuni build-package-v2 --payload F --out F [--law-version V]
 *	zmvuni build-aztec-v2 --package F --outdir D [--chunk-bytes N]
 *	zmvuni ingest --in P --type auto|package|xml|aztec|asg|media --out F
 *	zmvuni readiness [--fixtures D] --out F
 *	zmvuni -n ...		the falsifier: a package and a bundle trusted in what they say of themselves (payload
 *				length, package bytes, bundle name, chunk totals and indices unchecked)
 */
#include <u.h>
#include <libc.h>
#include <flate.h>

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


#define PACKAGE_V2 "ak.canonical.package.v2"
#define AZTEC_BUNDLE_V2 "ak.aztec.bundle.v2"
#define AZTEC_CHUNK_V2 "ak.aztec.chunk.v2"

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

static int
isobj(J *x)
{
	return x != nil && x->t == Jobj;
}

static J*
jarr(void)
{
	return mk(Jarr);
}

static J*
jnum(double n)
{
	J *j;

	j = mk(Jnum);
	j->n = n;
	return j;
}

static void
jsstr(J *d, J *j, int top)	/* String(v), onto d: an array's null and undefined elements empty */
{
	char b[64], *s;
	int i;

	if(j == nil){ if(top) addutf(d, "undefined"); return; }
	switch(j->t){
	case Jnull: if(top) addutf(d, "null"); return;
	case Jbool: addutf(d, j->b ? "true" : "false"); return;
	case Jnum: addutf(d, jsnum(j->n, b)); return;
	case Jstr: for(i = 0; i < j->ns; i++) addunit(d, j->s[i]); return;
	case Jobj: addutf(d, "[object Object]"); return;
	}
	for(i = 0; i < j->nv; i++){
		if(i) addunit(d, ',');
		jsstr(d, j->v[i], 0);
	}
	USED(s);
}

static J*
string(J *j)
{
	J *d;

	d = mk(Jstr);
	jsstr(d, j, 1);
	return d;
}

static J*
orstr(J *a, J *b)	/* String(a || b) */
{
	return string(truthy(a) ? a : b);
}

static double
jnumber(J *j)	/* Number(x) of the numbers and decimal strings these carry */
{
	if(j == nil) return NaN();
	if(j->t == Jnum) return j->n;
	if(j->t == Jstr) return number(j);
	if(j->t == Jnull) return 0;
	if(j->t == Jbool) return j->b;
	return NaN();
}

static void
ukeyset(J *o, char **want, char *ctx)	/* this tool's keyset: an object first */
{
	if(!isobj(o)) die(smprint("%s must be object", ctx));
	keyset(o, want, ctx);
}

static void
reqstring(J *v, char *ctx)
{
	if(v == nil || v->t != Jstr || v->ns == 0) die(smprint("%s invalid", ctx));
}

static char*
readtext(char *f, long *len)
{
	int fd;
	long n, l;
	char *s, buf[8192];

	if((fd = open(f, OREAD)) < 0)
		die(smprint("ENOENT: no such file or directory, open '%s'", f));
	s = strdup("");
	l = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, l + n + 1);
		memmove(s + l, buf, n);
		l += n;
		s[l] = 0;
	}
	close(fd);
	if(len) *len = l;
	return s;
}

static J*
parse(char *s)
{
	J *j;

	in = ip = s;
	j = value();
	ws();
	if(*ip) bad("Unexpected non-whitespace character after JSON");
	return j;
}

static J*
tryparse(char *s)	/* JSON.parse in a try: nil when it throws */
{
	J *j;

	catching = 1;
	if(setjmp(caught)){
		catching = 0;
		return nil;
	}
	j = parse(s);
	catching = 0;
	return j;
}

static void
mkdirs(char *path)	/* fs.mkdir(path, { recursive: true }) */
{
	char *p, *q;
	int fd;

	p = strdup(path);
	for(q = p; ; q++){
		if(*q == '/' || *q == 0){
			char c = *q;
			*q = 0;
			if(*p && access(p, AEXIST) < 0 && (fd = create(p, OREAD, DMDIR|0777)) >= 0)
				close(fd);
			*q = c;
			if(c == 0)
				break;
		}
	}
	free(p);
}

static void
writeout(char *f, J *j)	/* writeJson: its directory made first */
{
	char *d, *s;

	d = strdup(f);
	if((s = strrchr(d, '/')) != nil){
		*s = 0;
		mkdirs(d);
	}
	free(d);
	writejson(f, j);
}

static int
isdir(char *f)
{
	Dir *d;
	int r;

	if((d = dirstat(f)) == nil)
		die(smprint("ENOENT: no such file or directory, stat '%s'", f));
	r = (d->mode & DMDIR) != 0;
	free(d);
	return r;
}

/* ── the package ── */

static char *PKGKEYS[] = { "v", "authority", "manifest", "algorithms", "control_plane", "identity", "payload_bytes", nil };
static char *MANKEYS[] = { "type", "spec_version", "law_version", "section_order", nil };
static char *CPKEYS[] = { "encoding", "channels", "reserved", "canonicalization", nil };
static char *CHKEYS[] = { "FS", "GS", "RS", "US", nil };
static char *IDKEYS[] = { "payload_length", "payload_encoding", "source_result_hash", "source_replay_hash", "source_math_id_v2", nil };
static char *ORDER[] = { "manifest", "algorithms", "control_plane", "identity", "payload_bytes", nil };

static J*
strarr(char **s)
{
	J *a;
	int i;

	a = jarr();
	for(i = 0; s[i]; i++)
		push(a, mkstr(s[i]));
	return a;
}

static J*
pairs(char **kv)	/* an object of string pairs */
{
	J *o;
	int i;

	o = mk(Jobj);
	for(i = 0; kv[i]; i += 2)
		set(o, kv[i], mkstr(kv[i+1]));
	return o;
}

static J*
validatepackage(J *pkg)	/* validatePackageV2: the payload */
{
	J *m, *cp, *id, *pb, *b;
	uchar *raw;
	int i;

	ukeyset(pkg, PKGKEYS, "package");
	if(!eqs(get(pkg, "v"), PACKAGE_V2)) die("package.v mismatch");
	if(!eqs(get(pkg, "authority"), "advisory")) die("package authority must be advisory");
	m = get(pkg, "manifest");
	ukeyset(m, MANKEYS, "package.manifest");
	if(!eqs(get(m, "type"), PACKAGE_V2)) die("package.manifest.type mismatch");
	if(!eqs(get(m, "spec_version"), "ak.spec.v2")) die("package.manifest.spec_version mismatch");
	if(!same(get(m, "section_order"), strarr(ORDER))) die("package.manifest.section_order mismatch");
	cp = get(pkg, "control_plane");
	ukeyset(cp, CPKEYS, "package.control_plane");
	ukeyset(get(cp, "channels"), CHKEYS, "package.control_plane.channels");
	id = get(pkg, "identity");
	ukeyset(id, IDKEYS, "package.identity");
	if(!eqs(get(id, "payload_encoding"), "direct-bytes-v1")) die("package.identity.payload_encoding mismatch");
	pb = get(pkg, "payload_bytes");
	if(pb->t != Jarr) die("package.payload_bytes must be array");
	raw = malloc(pb->nv + 1);
	for(i = 0; i < pb->nv; i++){
		b = pb->v[i];
		if(b->t != Jnum || b->n != floor(b->n) || b->n < 0 || b->n > 255) die("package.payload_bytes value out of range");
		raw[i] = b->n;
	}
	raw[pb->nv] = 0;
	b = get(id, "payload_length");
	if(!trust && (b->t != Jnum || b->n != pb->nv)) die("package.payload_length mismatch");
	return parse((char*)raw);
}

static J*
buildpackage(J *payload, char *law)
{
	J *pkg, *m, *cp, *ch, *id, *pb;
	char *raw;
	int i, n;
	static char *alg[] = { "extract", "extract_control_stream.v1", "parse", "parse_orbit_channels.v1", "reduce", "reduce_orbit36.v1", "emit", "emit_propagation_artifact.v1", nil };
	static char *chs[] = { "FS", "0x1C", "GS", "0x1D", "RS", "0x1E", "US", "0x1F", nil };
	static char *res[] = { "0x3C", "0x3D", "0x3E", "0x3F", nil };

	raw = canon(payload);
	n = strlen(raw);
	m = mk(Jobj);
	set(m, "type", mkstr(PACKAGE_V2));
	set(m, "spec_version", mkstr("ak.spec.v2"));
	set(m, "law_version", mkstr(law));
	set(m, "section_order", strarr(ORDER));
	cp = mk(Jobj);
	set(cp, "encoding", mkstr("utf-ebcdic-control-v1"));
	ch = pairs(chs);
	set(cp, "channels", ch);
	set(cp, "reserved", strarr(res));
	set(cp, "canonicalization", mkstr("stream-sign-value-v1"));
	id = mk(Jobj);
	set(id, "payload_length", jnum(n));
	set(id, "payload_encoding", mkstr("direct-bytes-v1"));
	set(id, "source_result_hash", orstr(get(payload, "digest"), mkstr("")));
	set(id, "source_replay_hash", orstr(get(payload, "replay_hash"), mkstr("")));
	set(id, "source_math_id_v2", orstr(get(payload, "math_id_v2"), mkstr("")));
	pb = jarr();
	for(i = 0; i < n; i++)
		push(pb, jnum((uchar)raw[i]));
	pkg = mk(Jobj);
	set(pkg, "v", mkstr(PACKAGE_V2));
	set(pkg, "authority", mkstr("advisory"));
	set(pkg, "manifest", m);
	set(pkg, "algorithms", pairs(alg));
	set(pkg, "control_plane", cp);
	set(pkg, "identity", id);
	set(pkg, "payload_bytes", pb);
	return pkg;
}

static char*
pkgname(J *pkg)
{
	return smprint("package:%s", sval(get(get(pkg, "manifest"), "law_version")));
}

/* ── the bundle ── */

static char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char*
b64uenc(uchar *b, int n)
{
	char *s, *p;
	int i;
	ulong v;

	s = p = malloc(n / 3 * 4 + 5);
	for(i = 0; i + 2 < n; i += 3){
		v = b[i] << 16 | b[i+1] << 8 | b[i+2];
		*p++ = b64[v >> 18 & 63]; *p++ = b64[v >> 12 & 63]; *p++ = b64[v >> 6 & 63]; *p++ = b64[v & 63];
	}
	if(n - i == 1){
		v = b[i] << 16;
		*p++ = b64[v >> 18 & 63]; *p++ = b64[v >> 12 & 63];
	}else if(n - i == 2){
		v = b[i] << 16 | b[i+1] << 8;
		*p++ = b64[v >> 18 & 63]; *p++ = b64[v >> 12 & 63]; *p++ = b64[v >> 6 & 63];
	}
	*p = 0;
	return s;
}

static int
b64udec(char *s, uchar *out)	/* base64url, the padding implied */
{
	int n, bits, k;
	ulong v;
	char *c;

	n = bits = 0;
	v = 0;
	for(; *s; s++){
		if((c = strchr(b64, *s)) == nil || *s == 0)
			continue;
		v = v << 6 | (c - b64);
		bits += 6;
		if(bits >= 8){
			bits -= 8;
			out[n++] = v >> bits & 0xff;
		}
	}
	USED(k);
	return n;
}

/*
 * inflatezlibblock stops its input at the adler-32 trailer, and the inflater reads ahead past a block's end, so a
 * stream that ends tightly (zlib's at level 9) fails as an input error with its bytes all decoded; here the
 * deflate data is read with zeros after it for the look-ahead, and the trailer is checked against the output.
 */
typedef struct Src Src;
struct Src { uchar *p, *e; int pad; };
typedef struct Dst Dst;
struct Dst { uchar *p, *e; };

static int
srcget(void *v)
{
	Src *s;

	s = v;
	if(s->p < s->e)
		return *s->p++;
	if(s->pad-- > 0)
		return 0;
	return -1;
}

static int
dstwrite(void *v, void *buf, int n)
{
	Dst *d;

	d = v;
	if(d->e - d->p < n)
		return -1;
	memmove(d->p, buf, n);
	d->p += n;
	return n;
}

static int
zinflate(uchar *dst, int dsize, uchar *src, int ssize)
{
	Src s;
	Dst d;
	ulong a, want;

	if(ssize < 6 || ((src[0] << 8) | src[1]) % 31 != 0 || (src[0] & 0x0f) != 8)
		return -1;
	s.p = src + 2;
	s.e = src + ssize - 4;
	s.pad = 64;
	d.p = dst;
	d.e = dst + dsize;
	if(inflate(&d, dstwrite, &s, srcget) != FlateOk)
		return -1;
	a = adler32(1, dst, d.p - dst);
	want = (ulong)src[ssize-4] << 24 | (ulong)src[ssize-3] << 16 | (ulong)src[ssize-2] << 8 | src[ssize-1];
	if(a != want)
		return -1;
	return d.p - dst;
}

static char *AZKEYS[] = { "type", "v", "authority", "package_type", "payload_encoding", "bundle", "chunk_bytes", "total_chunks", "package_bytes", nil };
static char *CHUNKKEYS[] = { "type", "authority", "bundle", "index", "total", "data", nil };

static int
namecmp(void *a, void *b)
{
	return strcmp(*(char**)a, *(char**)b);
}

static J*
parseaztec(char *path, J **manifestp)	/* parseAztecBundleV2: the package */
{
	char *mpath, *cdir, *s, **names, *f, *enc;
	J *m, *chunks, *c, *t, *pkg;
	Dir *d;
	int fd, nd, nn, i, k, nb, n;
	uchar *comp, *out;
	double want;

	if(isdir(path)){
		mpath = smprint("%s/manifest.json", path);
		cdir = smprint("%s/chunks", path);
	}else{
		mpath = path;
		s = strdup(path);
		f = strrchr(s, '/');
		if(f){ *f = 0; cdir = smprint("%s/chunks", s); }
		else cdir = "chunks";
	}
	m = readjson(mpath);
	ukeyset(m, AZKEYS, "aztec.manifest");
	if(!eqs(get(m, "type"), AZTEC_BUNDLE_V2)) die("aztec.manifest.type mismatch");
	if(!eqs(get(m, "authority"), "advisory")) die("aztec.manifest authority must be advisory");
	if(!eqs(get(m, "package_type"), PACKAGE_V2)) die("aztec.manifest.package_type mismatch");
	if((fd = open(cdir, OREAD)) < 0) die(smprint("ENOENT: no such file or directory, scandir '%s'", cdir));
	nd = dirreadall(fd, &d);
	close(fd);
	names = malloc((nd + 1) * sizeof(char*));
	nn = 0;
	for(i = 0; i < nd; i++)
		if(strlen(d[i].name) >= 5 && strcmp(d[i].name + strlen(d[i].name) - 5, ".json") == 0)
			names[nn++] = strdup(d[i].name);
	qsort(names, nn, sizeof(char*), namecmp);
	if(nn != jnumber(get(m, "total_chunks"))) die("aztec chunk count mismatch");
	chunks = jarr();
	for(i = 0; i < nn; i++){
		c = readjson(smprint("%s/%s", cdir, names[i]));
		ukeyset(c, CHUNKKEYS, smprint("aztec.chunk:%s", names[i]));
		if(!eqs(get(c, "type"), AZTEC_CHUNK_V2)) die(smprint("aztec.chunk.type mismatch: %s", names[i]));
		if(!eqs(get(c, "authority"), "advisory")) die(smprint("aztec.chunk.authority mismatch: %s", names[i]));
		if(!trust && !same(get(c, "bundle"), get(m, "bundle"))) die(smprint("aztec.chunk.bundle mismatch: %s", names[i]));
		if(!trust && jnumber(get(c, "total")) != jnumber(get(m, "total_chunks"))) die(smprint("aztec.chunk.total mismatch: %s", names[i]));
		push(chunks, c);
	}
	for(i = 1; i < chunks->nv; i++)	/* by Number(index), stable */
		for(k = i; k > 0 && jnumber(get(chunks->v[k], "index")) < jnumber(get(chunks->v[k-1], "index")); k--){
			t = chunks->v[k]; chunks->v[k] = chunks->v[k-1]; chunks->v[k-1] = t;
		}
	for(i = 0; i < chunks->nv; i++)
		if(!trust && jnumber(get(chunks->v[i], "index")) != i) die("aztec.chunk index mismatch");
	enc = strdup("");
	for(i = 0; i < chunks->nv; i++){
		s = sval(get(chunks->v[i], "data"));
		enc = realloc(enc, strlen(enc) + strlen(s) + 1);
		strcat(enc, s);
	}
	comp = malloc(strlen(enc) + 1);
	n = b64udec(enc, comp);
	want = jnumber(get(m, "package_bytes"));
	nb = want > 0 && want < (1 << 24) ? (int)want : 0;
	nb = nb < (1 << 22) ? 1 << 22 : nb + 1;
	out = malloc(nb + 1);
	n = zinflate(out, nb, comp, n);
	if(n < 0) die("aztec.compressed data invalid");
	if(!trust && n != want) die("aztec.package_bytes mismatch");
	out[n] = 0;
	pkg = parse((char*)out);
	validatepackage(pkg);
	if(!trust && !eqs(get(m, "bundle"), pkgname(pkg))) die("aztec.bundle mismatch");
	if(manifestp) *manifestp = m;
	free(d);
	return pkg;
}

static void
buildaztec(J *pkg, char *outdir, int nb)
{
	char *bytes, *enc, *name;
	uchar *comp;
	int n, m, i, k, parts;
	J *mf, *c;

	validatepackage(pkg);
	bytes = canon(pkg);
	n = strlen(bytes);
	comp = malloc(n + 1024);
	m = deflatezlibblock(comp, n + 1024, (uchar*)bytes, n, 9, 0);
	if(m < 0) die("deflate failed");
	enc = b64uenc(comp, m);
	k = strlen(enc);
	parts = k == 0 ? 1 : (k + nb - 1) / nb;
	name = pkgname(pkg);
	mkdirs(smprint("%s/chunks", outdir));
	mf = mk(Jobj);
	set(mf, "type", mkstr(AZTEC_BUNDLE_V2));
	set(mf, "v", mkstr("ak.spec.v2"));
	set(mf, "authority", mkstr("advisory"));
	set(mf, "package_type", mkstr(PACKAGE_V2));
	set(mf, "payload_encoding", mkstr("zlib+base64url+package-bytes-v2"));
	set(mf, "bundle", mkstr(name));
	set(mf, "chunk_bytes", jnum(nb));
	set(mf, "total_chunks", jnum(parts));
	set(mf, "package_bytes", jnum(n));
	writeout(smprint("%s/manifest.json", outdir), mf);
	for(i = 0; i < parts; i++){
		char save, *at;
		c = mk(Jobj);
		set(c, "type", mkstr(AZTEC_CHUNK_V2));
		set(c, "authority", mkstr("advisory"));
		set(c, "bundle", mkstr(name));
		set(c, "index", jnum(i));
		set(c, "total", jnum(parts));
		at = enc + (long)i * nb;
		if((long)(i + 1) * nb < k){
			save = at[nb];
			at[nb] = 0;
			set(c, "data", mkstr(at));
			at[nb] = save;
		}else
			set(c, "data", mkstr(at));
		writeout(smprint("%s/chunks/chunk-%04d.json", outdir, i), c);
	}
	print("ok mv-universe-ingest build-aztec-v2 bundle=%s\n", name);
}

/* ── the scene ── */

static J*
handle(char *kind, char *def, J *node)
{
	J *h;

	h = mk(Jobj);
	set(h, "kind", mkstr(kind));
	set(h, "id", mkstr(smprint("projection:%s", kind)));
	set(h, "default", mkstr(def));
	set(h, "node_id", node);
	return h;
}

static J*
handles(char *type, char *media, J *node)	/* projectionDefaultsFor */
{
	J *a;

	a = jarr();
	if(strcmp(type, "package_v2") == 0 || strcmp(type, "aztec_bundle_v2") == 0){
		push(a, handle("graph", "1", node));
		push(a, handle("artifact_inspector", "0", node));
	}else if(strcmp(type, "xml_universe") == 0){
		push(a, handle("xml_universe", "1", node));
		push(a, handle("graph", "0", node));
	}else if(strcmp(type, "asg") == 0){
		push(a, handle("graph", "1", node));
		push(a, handle("svg", "0", node));
	}else if(strcmp(type, "media_ref") == 0){
		if(strcmp(media, "glb") == 0) push(a, handle("glb", "1", node));
		else if(strcmp(media, "wav") == 0) push(a, handle("wav", "1", node));
		else if(strcmp(media, "svg") == 0) push(a, handle("svg", "1", node));
		else push(a, handle("media", "1", node));
	}else
		push(a, handle("graph", "1", node));
	return a;
}

static J*
node(J *id, J *label, J *type, char *state, J *refs)
{
	J *n;

	n = mk(Jobj);
	set(n, "node_id", id);
	set(n, "label", label);
	set(n, "node_type", type);
	set(n, "verification_state", mkstr(state));
	set(n, "refs", refs ? refs : mk(Jobj));
	return n;
}

static J*
edge(int i, J *from, J *to, J *rel)
{
	J *e;

	e = mk(Jobj);
	set(e, "edge_id", mkstr(smprint("edge:%d:%s:%s", i, sval(from), sval(to))));
	set(e, "from", from);
	set(e, "to", to);
	set(e, "relation", rel);
	set(e, "authority", mkstr("advisory"));
	return e;
}

static int
inids(J *ids, J *x)	/* a Set of strings has x */
{
	int i;

	for(i = 0; i < ids->nv; i++)
		if(same(ids->v[i], x))
			return 1;
	return 0;
}

static int
isspace9(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int
namech(int c, int first)
{
	if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':') return 1;
	return !first && ((c >= '0' && c <= '9') || c == '-' || c == '.');
}

static J*
attrs(char *raw)	/* parseAttrs: name="value", the last of a name kept */
{
	J *a;
	char *p, *q, *nm, *v;
	int i, k;

	a = mk(Jobj);
	for(p = raw; *p; ){
		if(!namech((uchar)*p, 1)){ p++; continue; }
		q = p + 1;
		while(namech((uchar)*q, 0)) q++;
		k = q - p;
		while(isspace9(*q)) q++;
		if(*q != '='){ p++; continue; }
		q++;
		while(isspace9(*q)) q++;
		if(*q != '"'){ p++; continue; }
		v = ++q;
		while(*q && *q != '"') q++;
		if(*q != '"'){ p++; continue; }
		nm = malloc(k + 1);
		memmove(nm, p, k);
		nm[k] = 0;
		*q = 0;
		if(get(a, nm)){
			for(i = 0; i < a->nv; i++){
				char *kk = utf(a->k[i], a->nk[i]);
				if(strcmp(kk, nm) == 0) a->v[i] = mkstr(v);
			}
		}else
			set(a, nm, mkstr(v));
		*q = '"';
		p = q + 1;
	}
	return a;
}

static J*
tags(char *text, char *tag)	/* matchAll(/<tag\s+([^>]*?)\/>/g): each one's attributes */
{
	J *out;
	char *p, *q, *gt, *raw;
	int n;

	out = jarr();
	n = strlen(tag);
	for(p = text; (p = strstr(p, tag)) != nil; p++){
		q = p + n;
		if(!isspace9(*q)) continue;
		while(isspace9(*q)) q++;
		gt = strchr(q, '>');
		if(gt == nil || gt - 1 < q - 1 || gt[-1] != '/' || gt - 1 < p + n + 1) continue;
		raw = malloc(gt - q + 1);
		memmove(raw, q, gt - 1 - q > 0 ? gt - 1 - q : 0);
		raw[gt - 1 - q > 0 ? gt - 1 - q : 0] = 0;
		push(out, attrs(raw));
		p = gt;
	}
	return out;
}

static void
parsexml(char *raw, J **nodes, J **edges)
{
	char *t, *e;
	J *ns, *es, *a, *n, *ids, *x;
	int i;

	t = raw;
	while(isspace9(*t)) t++;
	t = strdup(t);
	for(e = t + strlen(t); e > t && isspace9(e[-1]); e--)
		;
	*e = 0;
	if(strncmp(t, "<universe", 9) != 0 || strstr(t, "</universe>") == nil) die("xml root must be <universe>...</universe>");
	ns = tags(t, "<node");
	es = tags(t, "<edge");
	*nodes = jarr();
	ids = jarr();
	for(i = 0; i < ns->nv; i++){
		a = ns->v[i];
		x = get(a, "id");
		if(!truthy(x)) die("xml node missing id");
		if(inids(ids, x)) die(smprint("xml duplicate node id: %s", sval(x)));
		push(ids, x);
		n = mk(Jobj);
		set(n, "id", x);
		set(n, "label", truthy(get(a, "label")) ? get(a, "label") : x);
		set(n, "type", truthy(get(a, "type")) ? get(a, "type") : mkstr("concept"));
		push(*nodes, n);
	}
	*edges = jarr();
	for(i = 0; i < es->nv; i++){
		a = es->v[i];
		if(!truthy(get(a, "from")) || !truthy(get(a, "to")) || !truthy(get(a, "relation"))) die("xml edge requires from,to,relation");
		if(!inids(ids, get(a, "from")) || !inids(ids, get(a, "to"))) die("xml edge references unknown node");
		n = mk(Jobj);
		set(n, "from", get(a, "from"));
		set(n, "to", get(a, "to"));
		set(n, "relation", get(a, "relation"));
		push(*edges, n);
	}
}

static void
parseasg(J *o, J **nodes, J **edges)
{
	J *ns, *es, *n, *e, *ids, *x;
	int i;

	if(!isobj(o)) die("asg must be object");
	ns = get(o, "nodes");
	es = get(o, "edges");
	if(ns == nil || ns->t != Jarr || es == nil || es->t != Jarr) die("asg must contain nodes[] and edges[]");
	ids = jarr();
	*nodes = jarr();
	for(i = 0; i < ns->nv; i++){
		n = ns->v[i];
		if(!isobj(n)) die(smprint("asg.nodes[%d] invalid", i));
		reqstring(get(n, "id"), smprint("asg.nodes[%d].id", i));
		if(inids(ids, get(n, "id"))) die(smprint("asg duplicate node id: %s", sval(get(n, "id"))));
		push(ids, get(n, "id"));
		x = mk(Jobj);
		set(x, "id", get(n, "id"));
		set(x, "label", orstr(get(n, "label"), get(n, "id")));
		set(x, "type", orstr(get(n, "type"), mkstr("node")));
		push(*nodes, x);
	}
	*edges = jarr();
	for(i = 0; i < es->nv; i++){
		e = es->v[i];
		if(!isobj(e)) die(smprint("asg.edges[%d] invalid", i));
		reqstring(get(e, "from"), smprint("asg.edges[%d].from", i));
		reqstring(get(e, "to"), smprint("asg.edges[%d].to", i));
		if(!inids(ids, get(e, "from")) || !inids(ids, get(e, "to"))) die(smprint("asg edge references unknown node at index %d", i));
		x = mk(Jobj);
		set(x, "from", get(e, "from"));
		set(x, "to", get(e, "to"));
		set(x, "relation", orstr(get(e, "kind"), mkstr("relates")));
		push(*edges, x);
	}
}

static char *MEDIAKEYS[] = { "v", "authority", "media_kind", "uri", "digest", nil };

static J*
ingest(char *path, char *type)
{
	J *scene, *ref, *ns, *es, *x, *pkg, *payload, *nodes, *edges, *refs, *nid, *canonical, *c, *m;
	char *raw, *src, *t;
	int dir, i;

	dir = isdir(path);
	raw = dir ? "" : readtext(path, nil);
	src = dir ? readtext(smprint("%s/manifest.json", path), nil) : raw;
	t = type;
	if(strcmp(type, "auto") == 0){
		if(strlen(path) >= 4 && strcmp(path + strlen(path) - 4, ".xml") == 0) t = "xml";
		else if(dir) t = "aztec";
		else {
			x = tryparse(raw);
			if(x == nil) t = "unknown";
			else if(eqs(get(x, "v"), PACKAGE_V2)) t = "package";
			else if(eqs(get(x, "type"), AZTEC_BUNDLE_V2)) t = "aztec";
			else if(truthy(get(x, "nodes")) && truthy(get(x, "edges"))) t = "asg";
			else if(eqs(get(x, "v"), "media.ref.v0")) t = "media";
			else t = "unknown";
		}
	}
	scene = mk(Jobj);
	set(scene, "v", mkstr("scene.model.v0"));
	set(scene, "authority", mkstr("advisory"));
	nodes = jarr();
	edges = jarr();
	if(strcmp(t, "package") == 0 || strcmp(t, "aztec") == 0){
		int az = strcmp(t, "aztec") == 0;
		pkg = az ? parseaztec(path, &m) : parse(raw);
		payload = validatepackage(pkg);
		nid = mkstr(pkgname(pkg));
		refs = mk(Jobj);
		set(refs, "source_replay_hash", orstr(get(payload, "replay_hash"), mkstr("")));
		set(refs, "source_result_hash", orstr(get(payload, "digest"), mkstr("")));
		push(nodes, node(nid, az ? mkstr("aztec:decoded-package") : mkstr(pkgname(pkg)), mkstr("canonical_package"), "verified", refs));
		set(scene, "verification_state", mkstr("verified"));
		set(scene, "source_type", mkstr(az ? "aztec_bundle_v2" : "package_v2"));
		set(scene, "projection_handles", handles(az ? "aztec_bundle_v2" : "package_v2", "", nid));
		canonical = pkg;
	}else if(strcmp(t, "xml") == 0 || strcmp(t, "asg") == 0){
		int xml = strcmp(t, "xml") == 0;
		if(xml)
			parsexml(raw, &ns, &es);
		else
			parseasg(parse(raw), &ns, &es);
		for(i = 0; i < ns->nv; i++)
			push(nodes, node(get(ns->v[i], "id"), get(ns->v[i], "label"), get(ns->v[i], "type"), "advisory", nil));
		for(i = 0; i < es->nv; i++)
			push(edges, edge(i, get(es->v[i], "from"), get(es->v[i], "to"), get(es->v[i], "relation")));
		set(scene, "verification_state", mkstr("advisory"));
		set(scene, "source_type", mkstr(xml ? "xml_universe" : "asg"));
		set(scene, "projection_handles", handles(xml ? "xml_universe" : "asg", "", ns->nv ? get(ns->v[0], "id") : mkstr(xml ? "xml:root" : "asg:root")));
		if(xml)
			canonical = mkstr(raw);
		else {
			canonical = mk(Jobj);
			set(canonical, "nodes", ns);
			set(canonical, "edges", es);
		}
	}else if(strcmp(t, "media") == 0){
		ref = parse(raw);
		ukeyset(ref, MEDIAKEYS, "media_ref");
		if(!eqs(get(ref, "v"), "media.ref.v0")) die("media_ref.v mismatch");
		if(!eqs(get(ref, "authority"), "advisory")) die("media_ref authority must be advisory");
		reqstring(get(ref, "media_kind"), "media_ref.media_kind");
		reqstring(get(ref, "uri"), "media_ref.uri");
		reqstring(get(ref, "digest"), "media_ref.digest");
		nid = mkstr(smprint("media:%s", sval(get(ref, "uri"))));
		refs = mk(Jobj);
		set(refs, "media_kind", get(ref, "media_kind"));
		set(refs, "uri", get(ref, "uri"));
		push(nodes, node(nid, get(ref, "uri"), mkstr("media"), "advisory", refs));
		set(scene, "verification_state", mkstr("advisory"));
		set(scene, "source_type", mkstr("media_ref"));
		set(scene, "projection_handles", handles("media_ref", sval(get(ref, "media_kind")), nid));
		canonical = ref;
	}else{
		die(smprint("unsupported input type: %s", t));
		return nil;
	}
	c = mk(Jobj);
	set(c, "source_path", mkstr(path));
	set(c, "source", mkstr(src));
	set(c, "canonical", canonical);
	set(scene, "artifact_ref", c);
	set(scene, "nodes", nodes);
	set(scene, "edges", edges);
	set(scene, "overlays", jarr());
	USED(m);
	return scene;
}

static void
readiness(char *fx, char *outf)
{
	static char *ids[] = { "package_v2", "xml_universe", "aztec_bundle_v2", "asg_artifact", "media_ref", nil };
	static char *files[] = { "sample-package.v2.json", "sample-universe.xml", "sample-aztec-v2", "sample-asg.json", "sample-media-ref.json", nil };
	static char *types[] = { "package", "xml", "aztec", "asg", "media", nil };
	J *flows, *f, *s, *h, *r, *b, *sum, *def;
	int i, k, ok;

	flows = jarr();
	ok = 0;
	for(i = 0; ids[i]; i++){
		s = ingest(smprint("%s/%s", fx, files[i]), types[i]);
		f = mk(Jobj);
		set(f, "entry_type", mkstr(ids[i]));
		set(f, "ok", mkstr("1"));
		set(f, "verification_state", get(s, "verification_state"));
		set(f, "source_type", get(s, "source_type"));
		def = mkstr("");
		h = get(s, "projection_handles");
		for(k = 0; k < h->nv; k++)
			if(eqs(get(h->v[k], "default"), "1")){
				def = get(h->v[k], "kind");
				break;
			}
		set(f, "default_projection", def);
		set(f, "node_count", mkstr(smprint("%d", get(s, "nodes")->nv)));
		set(f, "edge_count", mkstr(smprint("%d", get(s, "edges")->nv)));
		push(flows, f);
		ok++;
	}
	b = mk(Jobj);
	set(b, "ui_mutation_of_canonical_truth", mkstr("blocked"));
	set(b, "interaction_outputs", mkstr("overlay_or_proposal_only"));
	set(b, "canonical_write_path", mkstr("explicit_protocol_flow_only"));
	sum = mk(Jobj);
	set(sum, "total", mkstr("5"));
	set(sum, "ok", mkstr(smprint("%d", ok)));
	set(sum, "failed", mkstr(smprint("%d", 5 - ok)));
	r = mk(Jobj);
	set(r, "v", mkstr("universe.integration.readiness.v0"));
	set(r, "authority", mkstr("advisory"));
	set(r, "supported_entry_types", strarr(ids));
	set(r, "flows", flows);
	set(r, "authority_boundary", b);
	set(r, "summary", sum);
	writeout(outf, r);
	print("ok mv-universe-ingest readiness\n");
}

void
main(int argc, char *argv[])
{
	static char *flag[] = { "--in", "--out", "--outdir", "--type", "--fixtures", "--payload", "--package", "--law-version", "--chunk-bytes", nil };
	enum { In, Out, Outdir, Type, Fixtures, Payload, Package, Law, Chunk };
	char *val[nelem(flag)], *mode, *a, *v;
	int i, m;
	J *pkg;

	setfcr(getfcr() & ~(FPOVFL|FPZDIV|FPINVAL));
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		trust = 1;
		argc--;
		argv++;
	}
	deflateinit();
	inflateinit();
	memset(val, 0, sizeof val);
	val[Type] = "auto";
	val[Fixtures] = "fixtures/universe-integration";
	val[Chunk] = "1200";
	mode = nil;
	for(i = 1; i < argc; i++){	/* parseArgs: a flag takes the next when it is not empty */
		a = argv[i];
		v = i + 1 < argc && argv[i+1][0] ? argv[i+1] : nil;
		if(strcmp(a, "ingest") == 0 || strcmp(a, "readiness") == 0 || strcmp(a, "build-package-v2") == 0 || strcmp(a, "build-aztec-v2") == 0) mode = a;
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
		print("mv-universe-ingest build-package-v2 --payload <payload.json> --out <package.v2.json> [--law-version <v>]\n");
		print("mv-universe-ingest build-aztec-v2 --package <package.v2.json> --outdir <dir> [--chunk-bytes <n>]\n");
		print("mv-universe-ingest ingest --in <input> --type auto|package|xml|aztec|asg|media --out <scene.json>\n");
		print("mv-universe-ingest readiness --fixtures <fixtures/universe-integration> --out reports/integration/universe-integration-readiness.json\n");
		exits(nil);
	}
	if(strcmp(mode, "build-package-v2") == 0){
		if(!val[Payload] || !val[Out]) die("build-package-v2 requires --payload --out");
		pkg = buildpackage(readjson(val[Payload]), val[Law] ? val[Law] : "package-v2-draft");
		writeout(val[Out], pkg);
		print("ok mv-universe-ingest build-package-v2\n");
	}else if(strcmp(mode, "build-aztec-v2") == 0){
		if(!val[Package] || !val[Outdir]) die("build-aztec-v2 requires --package --outdir");
		buildaztec(readjson(val[Package]), val[Outdir], (int)strtod(val[Chunk], nil));
	}else if(strcmp(mode, "ingest") == 0){
		if(!val[In] || !val[Out]) die("ingest requires --in --out");
		writeout(val[Out], ingest(val[In], val[Type]));
		print("ok mv-universe-ingest ingest\n");
	}else{
		if(!val[Out]) die("readiness requires --out");
		readiness(val[Fixtures], val[Out]);
	}
	exits(nil);
}
