/*
 * zmvwave - Metaverse Kit's wave 24 to 26 tools in Plan 9 C, literal: mv-federate (two world graphs merged,
 * their relations by relation_id, a conflict resolved lexicographically, left or right), mv-provider-metric (a
 * provider's extension and its magnitude m = p + a + b) and mv-consumer-metric (a consumer's trace and its
 * q = lambda^2 - 2mn), each emit and validate, their checks in the tools' order and words. The tools stamp each
 * artifact with a sha256 of itself and name what it holds by digest; here every X_digest is X, the literal it
 * stood for (left and right the world graphs, merged the merged relations, a conflict's left_relation and
 * right_relation, a provider's canonical_world and extension, a trace's world), there is no digest of the
 * artifact, and validate recomputes and compares the literals. The inputs' own fields (a graph's digest and
 * base_world_entities_digest, the relation ids) are kept and compared as the tools compare them, as strings.
 * Numbers are JavaScript's: Number of a decimal string, a double; String of the sum or of q, Number's toString.
 *	zmvwave federate emit|validate --left F --right F [--strategy S] [--out F] [--merge F]
 *	zmvwave provider emit|validate --world-graph F [--seed F] [--out F] [--provider-extension F]
 *	zmvwave consumer emit|validate --world-graph F --provider-extension F [--seed F] [--out F] [--consumer-trace F]
 *	zmvwave -n ...		the falsifier: validate checks the form and trusts what is written, as a digest does
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

static char *embedded[] = { "left", "right", "merged", "canonical_world", "extension", "world", "left_relation", "right_relation", nil };

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

/* ── mv-federate ── */

static char *STRATEGIES[] = { "lexicographic", "left_preferred", "right_preferred", nil };
static char *RES[] = { "left", "right", "lexicographic", nil };
static char *RELKEYS[] = { "relation_id", "relation_type", "source_node", "stance", "target_node", "weight", nil };
static char *GRAPHKEYS[] = { "authority", "base_world_entities_digest", "digest", "relations", "summary", "v", nil };
static char *MERGEKEYS[] = { "authority", "conflict_summary", "left", "merged", "merged_relations", "rejected_components", "right", "strategy", "v", nil };
static char *CONFLICTKEYS[] = { "left_relation", "relation_id", "resolution", "right_relation", nil };

static char*
sval(J *v)
{
	return utf(v->s, v->ns);
}

static int
sorted(J *rels)	/* by relation_id; the ids are sha256 forms, so localeCompare is the code units' order */
{
	int i;

	for(i = 1; i < rels->nv; i++)
		if(cmp16(sval(get(rels->v[i-1], "relation_id")), sval(get(rels->v[i], "relation_id"))) > 0)
			return 0;
	return 1;
}

static void
validaterel(J *r, char *c)
{
	keyset(r, RELKEYS, c);
	reqsha(get(r, "relation_id"), smprint("%s.relation_id", c));
	reqsha(get(r, "source_node"), smprint("%s.source_node", c));
	reqsha(get(r, "target_node"), smprint("%s.target_node", c));
}

static void
validategraph(J *g, char *c)
{
	J *rels;
	int i;

	keyset(g, GRAPHKEYS, c);
	if(!eqs(get(g, "v"), "wave19.world_graph.v0")) die(smprint("%s version mismatch", c));
	if(!eqs(get(g, "authority"), "advisory")) die(smprint("%s authority mismatch", c));
	reqsha(get(g, "base_world_entities_digest"), smprint("%s.base_world_entities_digest", c));
	reqsha(get(g, "digest"), smprint("%s.digest", c));
	rels = get(g, "relations");
	if(rels->t != Jarr) die(smprint("%s.relations must be array", c));
	for(i = 0; i < rels->nv; i++)
		validaterel(rels->v[i], smprint("%s.relations[%d]", c, i));
	if(!sorted(rels)) die(smprint("%s.relations must be sorted", c));
}

static J*
lastof(J *rels, char *id)	/* new Map(rels by id): the last of an id */
{
	J *r;
	int i;

	r = nil;
	for(i = 0; i < rels->nv; i++)
		if(eqs(get(rels->v[i], "relation_id"), id))
			r = rels->v[i];
	return r;
}

static J*
make(J *left, J *right, J *strategy)
{
	J *L, *R, *merged, *conflicts, *l, *r, *chosen, *c, *m, *mo;
	char **ids, *id, *lc, *rc, *res;
	int n, i, k;

	L = get(left, "relations");
	R = get(right, "relations");
	ids = malloc((L->nv + R->nv + 1) * sizeof(char*));
	n = 0;
	for(i = 0; i < L->nv + R->nv; i++){
		id = sval(get(i < L->nv ? L->v[i] : R->v[i - L->nv], "relation_id"));
		for(k = 0; k < n && strcmp(ids[k], id) != 0; k++)
			;
		if(k == n)
			ids[n++] = id;
	}
	qsort(ids, n, sizeof(char*), sortstr);
	merged = mk(Jarr);
	conflicts = mk(Jarr);
	for(i = 0; i < n; i++){
		l = lastof(L, ids[i]);
		r = lastof(R, ids[i]);
		if(l && !r){ push(merged, l); continue; }
		if(!l && r){ push(merged, r); continue; }
		if(same(l, r)){ push(merged, l); continue; }
		if(eqs(strategy, "left_preferred")){ chosen = l; res = "left"; }
		else if(eqs(strategy, "right_preferred")){ chosen = r; res = "right"; }
		else {
			lc = canon(l);
			rc = canon(r);
			chosen = cmp16(lc, rc) <= 0 ? l : r;
			res = "lexicographic";
		}
		push(merged, chosen);
		c = mk(Jobj);
		set(c, "relation_id", mkstr(ids[i]));
		set(c, "left_relation", l);
		set(c, "right_relation", r);
		set(c, "resolution", mkstr(res));
		push(conflicts, c);
	}
	m = mk(Jobj);
	set(m, "authority", mkstr("advisory"));
	set(m, "conflict_summary", conflicts);
	set(m, "left", left);
	mo = mk(Jobj);
	set(mo, "relations", merged);
	set(m, "merged", mo);
	set(m, "merged_relations", merged);
	set(m, "rejected_components", mk(Jarr));
	set(m, "right", right);
	set(m, "strategy", strategy);
	set(m, "v", mkstr("wave24.federation_merge_result.v0"));
	return m;
}

static void
validatemerge(J *m, J *left, J *right)
{
	J *mr, *cs, *rj, *c, *rec;
	int i;
	char *x;

	keyset(m, MERGEKEYS, "federation_merge");
	if(!eqs(get(m, "v"), "wave24.federation_merge_result.v0")) die("federation merge version mismatch");
	if(!eqs(get(m, "authority"), "advisory")) die("federation merge authority must be advisory");
	if(!oneof(get(m, "strategy"), STRATEGIES)) die("strategy invalid");
	mr = get(m, "merged_relations");
	cs = get(m, "conflict_summary");
	rj = get(m, "rejected_components");
	if(mr->t != Jarr || cs->t != Jarr || rj->t != Jarr) die("merge arrays invalid");
	for(i = 0; i < mr->nv; i++)
		validaterel(mr->v[i], smprint("merged_relations[%d]", i));
	if(!sorted(mr)) die("merged_relations must be sorted");
	for(i = 0; i < cs->nv; i++){
		c = cs->v[i];
		keyset(c, CONFLICTKEYS, smprint("conflict_summary[%d]", i));
		reqsha(get(c, "relation_id"), smprint("conflict_summary[%d].relation_id", i));
		if(!oneof(get(c, "resolution"), RES)) die(smprint("conflict_summary[%d].resolution invalid", i));
	}
	x = canon(rj);
	if(strcmp(x, "[]\n") != 0) die("rejected_components must be empty in v0");
	if(!trust){
		rec = make(left, right, get(m, "strategy"));
		if(!same(get(m, "left"), left) || !same(get(m, "right"), right)) die("input literal mismatch");
		if(!same(get(rec, "merged_relations"), mr)) die("merged_relations mismatch");
		if(!same(get(rec, "conflict_summary"), cs)) die("conflict_summary mismatch");
		if(!same(get(rec, "merged"), get(m, "merged"))) die("merged mismatch");
	}
	reqmem(m, "federation_merge");
}

static void
federate(int argc, char **argv)
{
	static char *flags[] = { "--left", "--right", "--merge", "--strategy", "--out", nil };
	Arg a;
	J *left, *right, *m, *st;

	args(&a, argc, argv, flags);
	if(a.help || a.mode == nil){
		print("mv-federate emit --left <world_graph_a.json> --right <world_graph_b.json> [--strategy lexicographic] --out <federation_merge.v0.json>\n");
		print("mv-federate validate --left <world_graph_a.json> --right <world_graph_b.json> --merge <federation_merge.v0.json>\n");
		return;
	}
	if(a.val[0] == nil || a.val[1] == nil) die("--left and --right are required");
	st = mkstr(a.val[3] ? a.val[3] : "lexicographic");
	if(!oneof(st, STRATEGIES)) die("strategy invalid");
	left = readjson(a.val[0]);
	right = readjson(a.val[1]);
	validategraph(left, "left_world_graph");
	validategraph(right, "right_world_graph");
	if(!same(get(left, "base_world_entities_digest"), get(right, "base_world_entities_digest")))
		die("compatibility reject: base_world_entities_digest mismatch");
	if(strcmp(a.mode, "emit") == 0){
		if(a.val[4] == nil) die("emit requires --out");
		m = make(left, right, st);
		validatemerge(m, left, right);
		writejson(a.val[4], m);
		print("ok mv-federate emit\n");
		return;
	}
	if(a.val[2] == nil) die("validate requires --merge");
	m = readjson(a.val[2]);
	validatemerge(m, left, right);
	print("ok mv-federate validate\n");
}

/* ── mv-provider-metric ── */

static char *SCOPES[] = { "renderer", "adapter", "projection", "tooling", "hybrid", nil };
static char *WORLDKEYS[] = { "authority", "base_world_entities_digest", "digest", "relations", "summary", "v", nil };
static char *PSEEDKEYS[] = { "behavior_extension_count", "extension_artifact_count", "extension_scope", "projection_delta_size", "provider_id", nil };
static char *PROVKEYS[] = { "authority", "behavior_extension_count", "canonical_world", "extension", "extension_artifact_count", "extension_scope", "magnitude_m", "projection_delta_size", "provider_id", "v", nil };

static void
validateworld(J *g)
{
	keyset(g, WORLDKEYS, "world_graph");
	if(!eqs(get(g, "v"), "wave19.world_graph.v0")) die("world_graph version mismatch");
	reqsha(get(g, "digest"), "world_graph.digest");
}

static void
validatepseed(J *s)
{
	J *id;

	keyset(s, PSEEDKEYS, "seed");
	id = get(s, "provider_id");
	if(id->t != Jstr || id->ns == 0) die("seed.provider_id required");
	if(!oneof(get(s, "extension_scope"), SCOPES)) die("seed.extension_scope invalid");
	udec(get(s, "projection_delta_size"), "seed.projection_delta_size");
	udec(get(s, "extension_artifact_count"), "seed.extension_artifact_count");
	udec(get(s, "behavior_extension_count"), "seed.behavior_extension_count");
}

static J*
buildprovider(J *s, J *world)
{
	volatile double p, a, b, m;
	J *e, *x;

	validatepseed(s);
	p = udec(get(s, "projection_delta_size"), "seed.projection_delta_size");
	a = udec(get(s, "extension_artifact_count"), "seed.extension_artifact_count");
	b = udec(get(s, "behavior_extension_count"), "seed.behavior_extension_count");
	e = mk(Jobj);
	set(e, "behavior_extension_count", get(s, "behavior_extension_count"));
	set(e, "extension_artifact_count", get(s, "extension_artifact_count"));
	set(e, "extension_scope", get(s, "extension_scope"));
	set(e, "projection_delta_size", get(s, "projection_delta_size"));
	set(e, "provider_id", get(s, "provider_id"));
	m = p + a;
	m = m + b;
	x = mk(Jobj);
	set(x, "authority", mkstr("advisory"));
	set(x, "behavior_extension_count", get(s, "behavior_extension_count"));
	set(x, "canonical_world", world);
	set(x, "extension_artifact_count", get(s, "extension_artifact_count"));
	set(x, "extension", e);
	set(x, "extension_scope", get(s, "extension_scope"));
	set(x, "magnitude_m", numstr(m));
	set(x, "projection_delta_size", get(s, "projection_delta_size"));
	set(x, "provider_id", get(s, "provider_id"));
	set(x, "v", mkstr("wave25.provider_extension.v0"));
	return x;
}

static void
validateprovider(J *x, J *world)
{
	J *s, *rb;
	int i;

	keyset(x, PROVKEYS, "provider_extension");
	if(!eqs(get(x, "v"), "wave25.provider_extension.v0")) die("provider_extension version mismatch");
	if(!eqs(get(x, "authority"), "advisory")) die("provider_extension authority must be advisory");
	if(!oneof(get(x, "extension_scope"), SCOPES)) die("extension_scope invalid");
	udec(get(x, "projection_delta_size"), "projection_delta_size");
	udec(get(x, "extension_artifact_count"), "extension_artifact_count");
	udec(get(x, "behavior_extension_count"), "behavior_extension_count");
	udec(get(x, "magnitude_m"), "magnitude_m");
	if(!trust){
		if(!same(get(x, "canonical_world"), world)) die("canonical_world mismatch");
		s = mk(Jobj);
		for(i = 0; PSEEDKEYS[i]; i++)
			set(s, PSEEDKEYS[i], get(x, PSEEDKEYS[i]));
		rb = buildprovider(s, world);
		if(!same(get(rb, "extension"), get(x, "extension"))) die("extension mismatch");
		if(!same(get(rb, "magnitude_m"), get(x, "magnitude_m"))) die("magnitude_m mismatch");
	}
	reqmem(x, "provider_extension");
}

static void
provider(int argc, char **argv)
{
	static char *flags[] = { "--seed", "--world-graph", "--provider-extension", "--out", nil };
	Arg a;
	J *world, *x;

	args(&a, argc, argv, flags);
	if(a.help || a.mode == nil){
		print("mv-provider-metric emit --seed <provider.seed.json> --world-graph <world_graph.v0.json> --out <provider_extension.v0.json>\n");
		print("mv-provider-metric validate --provider-extension <provider_extension.v0.json> --world-graph <world_graph.v0.json>\n");
		return;
	}
	if(a.val[1] == nil) die("--world-graph is required");
	world = readjson(a.val[1]);
	validateworld(world);
	if(strcmp(a.mode, "emit") == 0){
		if(a.val[0] == nil || a.val[3] == nil) die("emit requires --seed --out");
		x = buildprovider(readjson(a.val[0]), world);
		validateprovider(x, world);
		writejson(a.val[3], x);
		print("ok mv-provider-metric emit\n");
		return;
	}
	if(a.val[2] == nil) die("validate requires --provider-extension");
	x = readjson(a.val[2]);
	validateprovider(x, world);
	print("ok mv-provider-metric validate\n");
}

/* ── mv-consumer-metric ── */

static char *CSEEDKEYS[] = { "behavior_invocations", "interaction_count", "invariant_axis_count", "max_projection_depth", nil };
static char *TRACEKEYS[] = { "authority", "behavior_invocations", "interaction_count", "invariant_axis_count", "max_projection_depth", "provider_magnitude_m", "q_value", "v", "world", nil };

static void
cprovider(J *p, J *world)	/* the consumer's reading of the provider */
{
	keyset(p, PROVKEYS, "provider_extension");
	if(!eqs(get(p, "v"), "wave25.provider_extension.v0")) die("provider_extension version mismatch");
	if(!trust && !same(get(p, "canonical_world"), world)) die("provider canonical_world mismatch");
	udec(get(p, "magnitude_m"), "provider_extension.magnitude_m");
}

static void
validatecseed(J *s)
{
	keyset(s, CSEEDKEYS, "seed");
	udec(get(s, "interaction_count"), "seed.interaction_count");
	udec(get(s, "max_projection_depth"), "seed.max_projection_depth");
	udec(get(s, "behavior_invocations"), "seed.behavior_invocations");
	udec(get(s, "invariant_axis_count"), "seed.invariant_axis_count");
}

static J*
buildtrace(J *s, J *world, J *mag)
{
	volatile double l, m, n, a, b, q;
	J *t;

	validatecseed(s);
	l = udec(get(s, "invariant_axis_count"), "seed.invariant_axis_count");
	m = udec(mag, "provider_magnitude_m");
	n = udec(get(s, "interaction_count"), "seed.interaction_count");
	a = l * l;	/* (lambda * lambda) - (2 * m * n), each product a double */
	b = 2 * m;
	b = b * n;
	q = a - b;
	t = mk(Jobj);
	set(t, "authority", mkstr("advisory"));
	set(t, "behavior_invocations", get(s, "behavior_invocations"));
	set(t, "interaction_count", get(s, "interaction_count"));
	set(t, "invariant_axis_count", get(s, "invariant_axis_count"));
	set(t, "max_projection_depth", get(s, "max_projection_depth"));
	set(t, "provider_magnitude_m", mag);
	set(t, "q_value", numstr(q));
	set(t, "v", mkstr("wave26.consumer_trace.v0"));
	set(t, "world", world);
	return t;
}

static void
validatetrace(J *t, J *world, J *mag)
{
	J *s, *rb;
	int i;

	keyset(t, TRACEKEYS, "consumer_trace");
	if(!eqs(get(t, "v"), "wave26.consumer_trace.v0")) die("consumer_trace version mismatch");
	if(!eqs(get(t, "authority"), "advisory")) die("consumer_trace authority must be advisory");
	udec(get(t, "interaction_count"), "interaction_count");
	udec(get(t, "max_projection_depth"), "max_projection_depth");
	udec(get(t, "behavior_invocations"), "behavior_invocations");
	udec(get(t, "invariant_axis_count"), "invariant_axis_count");
	udec(get(t, "provider_magnitude_m"), "provider_magnitude_m");
	idec(get(t, "q_value"), "q_value");
	if(!trust){
		if(!same(get(t, "world"), world)) die("world mismatch");
		if(!same(get(t, "provider_magnitude_m"), mag)) die("provider_magnitude_m mismatch");
		s = mk(Jobj);
		for(i = 0; CSEEDKEYS[i]; i++)
			set(s, CSEEDKEYS[i], get(t, CSEEDKEYS[i]));
		rb = buildtrace(s, world, mag);
		if(!same(get(rb, "q_value"), get(t, "q_value"))) die("q_value mismatch");
	}
	reqmem(t, "consumer_trace");
}

static void
logq(J *q)
{
	char *lv, *s, *p;

	if(!(number(q) < 0))
		return;
	lv = getenv("WAVE26_Q_LOG_LEVEL");
	if(lv == nil || *lv == 0)
		lv = "INFO";
	lv = strdup(lv);
	for(p = lv; *p; p++)
		if(*p >= 'a' && *p <= 'z')
			*p -= 'a' - 'A';
	s = sval(q);
	if(strcmp(lv, "WARN") == 0)
		fprint(2, "WARN: wave26 q_value<0 q=%s\n", s);
	else if(strcmp(lv, "QUIET") != 0 && strcmp(lv, "NONE") != 0)
		fprint(2, "INFO: wave26 q_value<0 q=%s\n", s);
}

static void
consumer(int argc, char **argv)
{
	static char *flags[] = { "--seed", "--world-graph", "--provider-extension", "--consumer-trace", "--out", nil };
	Arg a;
	J *world, *p, *t, *mag;

	args(&a, argc, argv, flags);
	if(a.help || a.mode == nil){
		print("mv-consumer-metric emit --seed <consumer.seed.json> --world-graph <world_graph.v0.json> --provider-extension <wave25.provider_extension.v0.json> --out <wave26.consumer_trace.v0.json>\n");
		print("mv-consumer-metric validate --consumer-trace <wave26.consumer_trace.v0.json> --world-graph <world_graph.v0.json> --provider-extension <wave25.provider_extension.v0.json>\n");
		return;
	}
	if(a.val[1] == nil || a.val[2] == nil) die("--world-graph and --provider-extension are required");
	world = readjson(a.val[1]);
	p = readjson(a.val[2]);
	validateworld(world);
	cprovider(p, world);
	mag = get(p, "magnitude_m");
	if(strcmp(a.mode, "emit") == 0){
		if(a.val[0] == nil || a.val[4] == nil) die("emit requires --seed --out");
		t = buildtrace(readjson(a.val[0]), world, mag);
		logq(get(t, "q_value"));
		validatetrace(t, world, mag);
		writejson(a.val[4], t);
		print("ok mv-consumer-metric emit\n");
		return;
	}
	if(a.val[3] == nil) die("validate requires --consumer-trace");
	t = readjson(a.val[3]);
	validatetrace(t, world, mag);
	logq(get(t, "q_value"));
	print("ok mv-consumer-metric validate\n");
}

void
main(int argc, char *argv[])
{
	setfcr(getfcr() & ~(FPOVFL|FPZDIV|FPINVAL));	/* JavaScript's doubles: a sum may be Infinity */
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		trust = 1;
		argc--;
		argv++;
	}
	if(argc < 2)
		sysfatal("usage: zmvwave [-n] federate|provider|consumer ...");
	if(strcmp(argv[1], "federate") == 0) federate(argc - 2, argv + 2);
	else if(strcmp(argv[1], "provider") == 0) provider(argc - 2, argv + 2);
	else if(strcmp(argv[1], "consumer") == 0) consumer(argc - 2, argv + 2);
	else sysfatal("unknown %s", argv[1]);
	exits(nil);
}
