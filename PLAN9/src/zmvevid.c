/*
 * zmvevid - Metaverse Kit's mv-evidence-bundle and mv-alignment in Plan 9 C, literal. The tools name what they
 * hold by sha256: a bundle its subject (a digest given on the command line) and each evidence artifact (by its
 * digest field), ordered by (v, digest); an alignment report its four inputs (narrative state, world entities,
 * world graph, behavior grammar) by the digest of each file's bytes; each artifact itself. Here a bundle holds
 * its subject and its evidence artifacts whole (an artifact needs no digest of its own: advisory, with a v),
 * ordered by v as localeCompare orders it and within a v by their canonical JSON's code units; a report holds
 * its four inputs whole; neither has a digest, and validate recomputes: a bundle's order, its artifacts and its
 * duplicates, a report's inputs' versions and its five checks, which the tool's validate cannot, holding only
 * their digests. The checks are the tool's: every behavior rule's nodes among the entities and the graph's
 * nodes; RELATE proposing a relation and UNRELATE an unrelation; every dialogue role one of five; the rules'
 * graph acyclic; and the projection order commuting, which sorts, reverses and sorts again and so cannot fail.
 *	zmvevid bundle build --subject F --claim-type T --evidence F... --out F
 *	zmvevid bundle verify --in F
 *	zmvevid align emit --narrative-state F --world-entities F --world-graph F --behavior-grammar F --out F
 *	zmvevid align validate --alignment F
 *	zmvevid -n ...		the falsifier: verify and validate check the form and trust what is written
 * localeCompare is ICU's collation; here it is read for printable ASCII (punctuation, digits, letters a A b B, the
 * case only where all else is equal), as node orders it, and other strings by their code units.
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

static char **embedded;	/* the literals of an artifact's inputs, which the string membrane does not cover */

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


/* localeCompare for printable ASCII: the primary order node's ICU gives, then lower case before upper */
static char COLL[] = " _-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$0123456789abcdefghijklmnopqrstuvwxyz";

static int
prim(int c)
{
	if(c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	return strchr(COLL, c) - COLL;
}

static int
collate(J *a, J *b)
{
	int i, n, x, y;

	for(i = 0; i < a->ns; i++) if(a->s[i] < 0x20 || a->s[i] > 0x7e) return ucmp(a->s, a->ns, b->s, b->ns);
	for(i = 0; i < b->ns; i++) if(b->s[i] < 0x20 || b->s[i] > 0x7e) return ucmp(a->s, a->ns, b->s, b->ns);
	n = a->ns < b->ns ? a->ns : b->ns;
	for(i = 0; i < n; i++){
		x = prim(a->s[i]);
		y = prim(b->s[i]);
		if(x != y)
			return x < y ? -1 : 1;
	}
	if(a->ns != b->ns)
		return a->ns < b->ns ? -1 : 1;
	for(i = 0; i < n; i++){
		x = a->s[i] >= 'A' && a->s[i] <= 'Z';
		y = b->s[i] >= 'A' && b->s[i] <= 'Z';
		if(x != y)
			return x < y ? -1 : 1;
	}
	return 0;
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
orempty(J *j, char *expr)	/* x || [], iterated: an array, or nothing */
{
	if(!truthy(j))
		return mk(Jarr);
	if(j->t != Jarr)
		die(smprint("%s is not iterable", expr));
	return j;
}

static J*
field(J *o, char *k)	/* o.k: undefined (nil) off a primitive, a TypeError off null */
{
	if(o == nil)
		die(smprint("Cannot read properties of undefined (reading '%s')", k));
	if(o->t == Jnull)
		die(smprint("Cannot read properties of null (reading '%s')", k));
	return get(o, k);
}

/* ── mv-evidence-bundle ── */

static char *BUNDLEKEYS[] = { "authority", "claim_type", "evidence", "subject", "v", nil };
static char *bundleskip[] = { "subject", "evidence", nil };

static int
evcmp(J *a, J *b)	/* v by localeCompare, then the canonical JSON's code units */
{
	J *va, *vb;

	va = get(a, "v");
	vb = get(b, "v");
	if(ueq(va->s, va->ns, vb->s, vb->ns))
		return cmp16(canon(a), canon(b));
	return collate(va, vb);
}

static J*
evsort(J *ev)
{
	J *s, *t;
	int i, m;

	s = mk(Jarr);
	for(i = 0; i < ev->nv; i++)
		push(s, ev->v[i]);
	for(i = 1; i < s->nv; i++)
		for(m = i; m > 0 && evcmp(s->v[m], s->v[m-1]) < 0; m--){
			t = s->v[m]; s->v[m] = s->v[m-1]; s->v[m-1] = t;
		}
	return s;
}

static int
evdup(J *ev)
{
	int i, k;

	for(i = 0; i < ev->nv; i++)
		for(k = 0; k < i; k++)
			if(same(ev->v[i], ev->v[k]))
				return 1;
	return 0;
}

static void
validatebundle(J *b)
{
	J *ev, *x, *v, *c;
	int i;

	keyset(b, BUNDLEKEYS, "evidence_bundle");
	if(!eqs(get(b, "v"), "wave30.evidence_bundle.v0")) die("evidence_bundle version mismatch");
	if(!eqs(get(b, "authority"), "advisory")) die("evidence_bundle authority must be advisory");
	c = get(b, "claim_type");
	if(c->t != Jstr || c->ns == 0) die("evidence_bundle.claim_type invalid");
	ev = get(b, "evidence");
	if(ev->t != Jarr || ev->nv == 0) die("evidence_bundle evidence must be non-empty array");
	if(!trust){
		for(i = 0; i < ev->nv; i++){
			x = ev->v[i];
			if(x->t != Jobj && x->t != Jarr) die(smprint("evidence[%d] not object", i));
			v = get(x, "v");
			if(v == nil || v->t != Jstr || v->ns == 0) die(smprint("evidence[%d].v invalid", i));
			if(!eqs(get(x, "authority"), "advisory")) die(smprint("evidence[%d] authority must be advisory", i));
		}
		if(!same(evsort(ev), ev)) die("evidence_bundle evidence ordering mismatch");
		if(evdup(ev)) die("evidence_bundle evidence contains duplicates");
	}
	embedded = bundleskip;
	reqmem(b, "evidence_bundle");
}

static void
bundle(int argc, char **argv)
{
	char *subject, *claim, *outf, *inf, *ev[256];
	int nev, i, help;
	char *mode;
	J *b, *x, *refs, *s;

	subject = claim = outf = inf = mode = nil;
	nev = help = 0;
	for(i = 0; i < argc; i++){	/* parseArgs: a flag takes the next when it is not empty; --evidence again and again */
		if(strcmp(argv[i], "build") == 0) mode = "build";
		else if(strcmp(argv[i], "verify") == 0) mode = "verify";
		else if(strcmp(argv[i], "--subject") == 0 && i + 1 < argc && argv[i+1][0]) subject = argv[++i];
		else if(strcmp(argv[i], "--claim-type") == 0 && i + 1 < argc && argv[i+1][0]) claim = argv[++i];
		else if(strcmp(argv[i], "--evidence") == 0 && i + 1 < argc && argv[i+1][0] && nev < nelem(ev)) ev[nev++] = argv[++i];
		else if(strcmp(argv[i], "--out") == 0 && i + 1 < argc && argv[i+1][0]) outf = argv[++i];
		else if(strcmp(argv[i], "--in") == 0 && i + 1 < argc && argv[i+1][0]) inf = argv[++i];
		else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) help = 1;
		else die(smprint("unknown arg: %s", argv[i]));
	}
	if(help || mode == nil){
		print("mv-evidence-bundle build --subject <subject.json> --claim-type <type> --evidence <artifact.json>... --out <bundle.json>\n");
		print("mv-evidence-bundle verify --in <bundle.json>\n");
		return;
	}
	if(strcmp(mode, "build") == 0){
		if(subject == nil || claim == nil || outf == nil || nev == 0)
			die("build requires --subject --claim-type --evidence... --out");
		s = readjson(subject);
		refs = mk(Jarr);
		for(i = 0; i < nev; i++){
			x = readjson(ev[i]);
			if(!truthy(x) || (x->t != Jobj && x->t != Jarr)) die(smprint("evidence artifact not object: %s", ev[i]));
			if(!eqs(get(x, "authority"), "advisory")) die(smprint("evidence artifact authority must be advisory: %s", ev[i]));
			if(get(x, "v") == nil || get(x, "v")->t != Jstr) die(smprint("evidence artifact missing v: %s", ev[i]));
			push(refs, x);
		}
		refs = evsort(refs);
		if(evdup(refs)) die("evidence refs contain duplicates");
		b = mk(Jobj);
		set(b, "authority", mkstr("advisory"));
		set(b, "claim_type", mkstr(claim));
		set(b, "evidence", refs);
		set(b, "subject", s);
		set(b, "v", mkstr("wave30.evidence_bundle.v0"));
		validatebundle(b);
		writejson(outf, b);
		print("ok mv-evidence-bundle build\n");
		return;
	}
	if(inf == nil) die("verify requires --in");
	b = readjson(inf);
	validatebundle(b);
	print("ok mv-evidence-bundle verify\n");
}

/* ── mv-alignment ── */

static char *REPORTKEYS[] = { "authority", "behavior_grammar", "checks", "narrative_state", "projection_order", "status", "v", "world_entities", "world_graph", nil };
static char *CHECKKEYS[] = { "acyclic_behavior_graph", "behavior_effects_supported", "behavior_targets_resolve", "narrative_roles_resolve", "projection_order_commutes", nil };
static char *ROLES[] = { "solon", "solomon", "asabiyyah", "metatron", "witness", nil };
static char *ORDER[] = { "behavior_grammar", "narrative_state", "world_entities", "world_graph", nil };
static char *reportskip[] = { "narrative_state", "world_entities", "world_graph", "behavior_grammar", nil };

static int
eqv(J *a, J *b)	/* SameValueZero; nil is undefined */
{
	if(a == nil || b == nil) return a == b;
	if(a->t != b->t) return 0;
	switch(a->t){
	case Jnull: return 1;
	case Jbool: return a->b == b->b;
	case Jnum: return a->n == b->n;
	case Jstr: return ueq(a->s, a->ns, b->s, b->ns);
	}
	return a == b;
}

typedef struct Set Set;
struct Set { J **v; int n; };

static int
has(Set *s, J *x)
{
	int i;

	for(i = 0; i < s->n; i++)
		if(eqv(s->v[i], x))
			return i + 1;
	return 0;
}

static void
add(Set *s, J *x)
{
	if(has(s, x))
		return;
	s->v = realloc(s->v, (s->n + 1) * sizeof(J*));
	s->v[s->n++] = x;
}

static int adjn[256][256], nadj[256], seen[256], onstack[256];

static int
dfs(int n)
{
	int k;

	if(onstack[n]) return 0;
	if(seen[n]) return 1;
	seen[n] = onstack[n] = 1;
	for(k = 0; k < nadj[n]; k++)
		if(!dfs(adjn[n][k]))
			return 0;
	onstack[n] = 0;
	return 1;
}

static int
acyclic(J *rules)
{
	Set nodes;
	J *r;
	int i, s, t;

	memset(&nodes, 0, sizeof nodes);
	memset(nadj, 0, sizeof nadj);
	memset(seen, 0, sizeof seen);
	memset(onstack, 0, sizeof onstack);
	for(i = 0; i < rules->nv; i++){
		r = rules->v[i];
		add(&nodes, field(r, "source_node"));
		add(&nodes, field(r, "target_node"));
		s = has(&nodes, get(r, "source_node")) - 1;
		t = has(&nodes, get(r, "target_node")) - 1;
		if(nodes.n >= nelem(nadj) || nadj[s] >= nelem(adjn[0])) die("behavior graph too large");
		adjn[s][nadj[s]++] = t;
	}
	for(i = 0; i < nodes.n; i++)
		if(!dfs(i))
			return 0;
	return 1;
}

static int
commutes(J **in)	/* projectionCommutes: sorted, reversed and sorted again, it is the sorted: always */
{
	char *a[4], *b[4];
	int i, k;

	for(i = 0; i < 4; i++)
		a[i] = canon(in[i]);
	qsort(a, 4, sizeof(char*), sortstr);
	for(i = 0; i < 4; i++)
		b[i] = a[3 - i];
	qsort(b, 4, sizeof(char*), sortstr);
	for(i = 0, k = 1; i < 4; i++)
		if(strcmp(a[i], b[i]) != 0)
			k = 0;
	return k;
}

static J*
align(J *n, J *e, J *g, J *b)	/* the versions and the five checks, as mv-alignment emit computes them */
{
	Set ents, gnodes;
	J *rules, *x, *r, *states, *roles, *c, *in[4];
	int i, k, tr, ef, ro, ac, pc;
	char *s;

	if(!eqs(field(n, "v"), "wave16.narrative_state.v0")) die("narrative_state version mismatch");
	if(!eqs(field(e, "v"), "wave19.world_entities.v0")) die("world_entities version mismatch");
	if(!eqs(field(g, "v"), "wave19.world_graph.v0")) die("world_graph version mismatch");
	if(!eqs(field(b, "v"), "wave20.behavior_grammar.v0")) die("behavior_grammar version mismatch");
	memset(&ents, 0, sizeof ents);
	memset(&gnodes, 0, sizeof gnodes);
	x = orempty(get(e, "entities"), "e.entities || []");
	for(i = 0; i < x->nv; i++)
		add(&ents, field(x->v[i], "node_id"));
	x = orempty(get(g, "relations"), "g.relations || []");
	for(i = 0; i < x->nv; i++){
		add(&gnodes, field(x->v[i], "source_node"));
		add(&gnodes, field(x->v[i], "target_node"));
	}
	rules = orempty(get(b, "rules"), "behavior.rules || []");
	tr = 1;
	for(i = 0; i < rules->nv && tr; i++){
		r = rules->v[i];
		if(!has(&ents, field(r, "source_node")) || !has(&ents, get(r, "target_node")) || !has(&gnodes, get(r, "source_node")) || !has(&gnodes, get(r, "target_node")))
			tr = 0;
	}
	ef = 1;
	for(i = 0; i < rules->nv && ef; i++){
		r = rules->v[i];
		if(eqs(field(r, "verb"), "RELATE") && !eqs(get(r, "effect"), "propose_relation")) ef = 0;
		if(eqs(get(r, "verb"), "UNRELATE") && !eqs(get(r, "effect"), "propose_unrelation")) ef = 0;
	}
	ro = 1;
	states = orempty(get(n, "states"), "narrative.states || []");
	for(i = 0; i < states->nv && ro; i++){
		roles = orempty(field(states->v[i], "dialogue_roles"), "s.dialogue_roles || []");
		for(k = 0; k < roles->nv; k++)
			if(!oneof(roles->v[k], ROLES)){
				ro = 0;
				break;
			}
	}
	ac = acyclic(rules);
	in[0] = n; in[1] = e; in[2] = g; in[3] = b;
	pc = commutes(in);
	c = mk(Jobj);	/* in the tool's order, for its message */
	set(c, "behavior_targets_resolve", mkstr(tr ? "1" : "0"));
	set(c, "behavior_effects_supported", mkstr(ef ? "1" : "0"));
	set(c, "narrative_roles_resolve", mkstr(ro ? "1" : "0"));
	set(c, "acyclic_behavior_graph", mkstr(ac ? "1" : "0"));
	set(c, "projection_order_commutes", mkstr(pc ? "1" : "0"));
	if(!(tr && ef && ro && ac && pc)){
		s = smprint("alignment failed checks: {\"behavior_targets_resolve\":\"%d\",\"behavior_effects_supported\":\"%d\",\"narrative_roles_resolve\":\"%d\",\"acyclic_behavior_graph\":\"%d\",\"projection_order_commutes\":\"%d\"}", tr, ef, ro, ac, pc);
		die(s);
	}
	return c;
}

static void
validatereport(J *r)
{
	J *c, *po, *ord;
	int i;

	keyset(r, REPORTKEYS, "alignment");
	if(!eqs(get(r, "v"), "wave21.alignment_report.v0")) die("alignment version mismatch");
	if(!eqs(get(r, "authority"), "advisory")) die("alignment authority must be advisory");
	if(!eqs(get(r, "status"), "valid")) die("alignment status must be valid");
	c = get(r, "checks");
	keyset(c, CHECKKEYS, "checks");
	for(i = 0; i < c->nv; i++)
		if(!eqs(c->v[i], "0") && !eqs(c->v[i], "1")) die("checks must be 0|1");
	for(i = 0; i < c->nv; i++)
		if(!eqs(c->v[i], "1")) die("alignment contains failed checks");
	po = get(r, "projection_order");
	ord = mk(Jarr);
	for(i = 0; ORDER[i]; i++)
		push(ord, mkstr(ORDER[i]));
	if(!same(po, ord)) die("projection_order mismatch");
	embedded = reportskip;
	reqmem(r, "alignment");
	if(!trust)
		align(get(r, "narrative_state"), get(r, "world_entities"), get(r, "world_graph"), get(r, "behavior_grammar"));
}

static void
alignment(int argc, char **argv)
{
	static char *flags[] = { "--narrative-state", "--world-entities", "--world-graph", "--behavior-grammar", "--alignment", "--out", nil };
	Arg a;
	J *n, *e, *g, *b, *c, *r, *ord;
	int i;

	args(&a, argc, argv, flags);
	if(a.help || a.mode == nil){
		print("mv-alignment emit --narrative-state <...> --world-entities <...> --world-graph <...> --behavior-grammar <...> --out <alignment.json>\n");
		print("mv-alignment validate --alignment <alignment.json>\n");
		return;
	}
	if(strcmp(a.mode, "emit") == 0){
		for(i = 0; i < 4; i++)
			if(a.val[i] == nil) break;
		if(i < 4 || a.val[5] == nil) die("emit requires --narrative-state --world-entities --world-graph --behavior-grammar --out");
		n = readjson(a.val[0]);
		e = readjson(a.val[1]);
		g = readjson(a.val[2]);
		b = readjson(a.val[3]);
		c = align(n, e, g, b);
		r = mk(Jobj);
		set(r, "authority", mkstr("advisory"));
		set(r, "behavior_grammar", b);
		set(r, "checks", c);
		set(r, "narrative_state", n);
		ord = mk(Jarr);
		for(i = 0; ORDER[i]; i++)
			push(ord, mkstr(ORDER[i]));
		set(r, "projection_order", ord);
		set(r, "status", mkstr("valid"));
		set(r, "v", mkstr("wave21.alignment_report.v0"));
		set(r, "world_entities", e);
		set(r, "world_graph", g);
		validatereport(r);
		writejson(a.val[5], r);
		print("ok mv-alignment emit\n");
		return;
	}
	if(a.val[4] == nil) die("validate requires --alignment");
	r = readjson(a.val[4]);
	validatereport(r);
	print("ok mv-alignment validate\n");
}

void
main(int argc, char *argv[])
{
	setfcr(getfcr() & ~(FPOVFL|FPZDIV|FPINVAL));
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		trust = 1;
		argc--;
		argv++;
	}
	if(argc < 2)
		sysfatal("usage: zmvevid [-n] bundle|align ...");
	if(strcmp(argv[1], "bundle") == 0) bundle(argc - 2, argv + 2);
	else if(strcmp(argv[1], "align") == 0) alignment(argc - 2, argv + 2);
	else sysfatal("unknown %s", argv[1]);
	exits(nil);
}
