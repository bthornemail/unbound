/*
 * zportmat - Metaverse Kit's mv-portmatroid-project (tools/mv-portmatroid-project/index.js) in Plan 9 C:
 * a port-matroid snapshot, as JSON, projected to its view: kind, the snapshot's tick, hash_hex and
 * entity_count, and a node for each entity (node_id "ent:" and String(eid), etype, components ?? {}),
 * written as the tool's stableStringify writes it: keys sorted by UTF-16 code units, strings as
 * JSON.stringify escapes them, numbers as Number's toString, a missing field as the string "undefined".
 *	zportmat --snapshot-json FILE [--out FILE]
 *	zportmat -n ...		the falsifier: a missing field left out, as JSON.stringify leaves it
 * The tool's --snapshot FILE.csnp runs cabal on the host to export that JSON; here it is refused.
 * Strings are held as UTF-16 code units, as JavaScript holds them, so a lone surrogate keeps its escape.
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

static int dropundef;
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

static void
stable(J *j)
{
	char b[64];
	int i, m, *ord, t;

	switch(j->t){
	case Jundef: putlit("undefined"); return;
	case Jnull: puts1("null"); return;
	case Jbool: puts1(j->b ? "true" : "false"); return;
	case Jnum:
		jsnum(j->n, b);
		puts1(strchr(b, 'I') || strchr(b, 'N') ? "null" : b);	/* JSON.stringify of a non-finite */
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
	m = 0;
	for(i = 0; i < j->nv; i++){
		if(dropundef && j->v[ord[i]]->t == Jundef)
			continue;
		if(m++) put(",", 1);
		putunits(j->k[ord[i]], j->nk[ord[i]], 1);
		put(":", 1);
		stable(j->v[ord[i]]);
	}
	put("}", 1);
	free(ord);
}

static void
addascii(J *d, char *s)
{
	while(*s)
		addunit(d, *s++);
}

static void
jsstring(J *d, J *j, int top)	/* String(v), onto d's units; an array's null and undefined elements are empty */
{
	char b[64];
	int i;

	switch(j->t){
	case Jundef: if(top) addascii(d, "undefined"); return;
	case Jnull: if(top) addascii(d, "null"); return;
	case Jbool: addascii(d, j->b ? "true" : "false"); return;
	case Jnum: addascii(d, jsnum(j->n, b)); return;
	case Jstr: for(i = 0; i < j->ns; i++) addunit(d, j->s[i]); return;
	case Jobj: addascii(d, "[object Object]"); return;
	}
	for(i = 0; i < j->nv; i++){
		if(i) addunit(d, ',');
		jsstring(d, j->v[i], 0);
	}
}

static J undef = { Jundef };

static ushort*
units(char *s, int *n)
{
	ushort *u;
	int i;

	*n = strlen(s);
	u = malloc((*n + 1) * sizeof(ushort));
	for(i = 0; i < *n; i++)
		u[i] = s[i];
	return u;
}

static J*
get(J *o, char *key)
{
	ushort *u;
	int n, i;

	if(o == nil || o->t != Jobj)
		return &undef;
	u = units(key, &n);
	for(i = 0; i < o->nv; i++)
		if(ueq(o->k[i], o->nk[i], u, n)){
			free(u);
			return o->v[i];
		}
	free(u);
	return &undef;
}

static void
set(J *o, char *key, J *v)
{
	o->nv++;
	o->k = realloc(o->k, o->nv * sizeof(ushort*));
	o->nk = realloc(o->nk, o->nv * sizeof(int));
	o->v = realloc(o->v, o->nv * sizeof(J*));
	o->k[o->nv-1] = units(key, &o->nk[o->nv-1]);
	o->v[o->nv-1] = v;
}

static J*
project(J *snap)
{
	J *p, *s, *nodes, *ents, *e, *n, *c, *id;
	int i;

	if(snap->t == Jnull || snap->t == Jundef){
		fprint(2, "TypeError: Cannot read properties of null (reading 'entities')\n");
		exits("null");
	}
	ents = get(snap, "entities");
	p = mk(Jobj);
	id = mk(Jstr);
	addascii(id, "metaverse-kit.projector.port_matroid.v0");
	set(p, "kind", id);
	s = mk(Jobj);
	set(s, "tick", get(snap, "tick"));
	set(s, "hash_hex", get(snap, "hash_hex"));
	set(s, "entity_count", get(snap, "entity_count"));
	set(p, "snapshot", s);
	nodes = mk(Jarr);
	for(i = 0; ents->t == Jarr && i < ents->nv; i++){
		e = ents->v[i];
		if(e->t == Jnull){
			fprint(2, "TypeError: Cannot read properties of null (reading 'eid')\n");
			exits("null");
		}
		n = mk(Jobj);
		id = mk(Jstr);
		addascii(id, "ent:");
		jsstring(id, get(e, "eid"), 1);
		set(n, "node_id", id);
		set(n, "etype", get(e, "etype"));
		c = get(e, "components");
		if(c->t == Jnull || c->t == Jundef)
			c = mk(Jobj);
		set(n, "components", c);
		nodes->v = realloc(nodes->v, (nodes->nv + 1) * sizeof(J*));
		nodes->v[nodes->nv++] = n;
	}
	set(p, "nodes", nodes);
	return p;
}

static char*
readfile(char *f)
{
	int fd;
	long n, len;
	char *s, buf[8192];

	if((fd = open(f, OREAD)) < 0){
		fprint(2, "Error: ENOENT: no such file or directory, open '%s'\n", f);
		exits("open");
	}
	s = strdup("");
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, len + n + 1);
		memmove(s + len, buf, n);
		len += n;
		s[len] = 0;
	}
	close(fd);
	return s;
}

void
main(int argc, char *argv[])
{
	char *json, *ofile;
	J *snap;
	int i, fd;

	json = ofile = nil;
	if(argc > 1 && strcmp(argv[1], "-n") == 0){
		dropundef = 1;
		argc--; argv++;
	}
	for(i = 1; i < argc; i++){	/* parseArgs: a flag takes the next argument when that is not empty */
		if(strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc && argv[i+1][0]){
			fprint(2, "Error: --snapshot runs cabal on the host: export the snapshot's JSON there\n");
			exits("csnp");
		}
		else if(strcmp(argv[i], "--snapshot-json") == 0 && i + 1 < argc && argv[i+1][0]) json = argv[++i];
		else if(strcmp(argv[i], "--out") == 0 && i + 1 < argc && argv[i+1][0]) ofile = argv[++i];
		else if(strcmp(argv[i], "--pm-root") == 0 && i + 1 < argc && argv[i+1][0]) i++;
	}
	if(json == nil){
		fprint(2, "Error: usage: mv-portmatroid-project --snapshot <file.csnp> | --snapshot-json <file.json> [--out <file>]\n");
		exits("usage");
	}
	in = ip = readfile(json);
	snap = value();
	ws();
	if(*ip) bad("Unexpected non-whitespace character after JSON");
	stable(project(snap));
	put("\n", 1);
	if(ofile){
		if((fd = create(ofile, OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", ofile);
		write(fd, out, nout);
		close(fd);
		print("Wrote projection to %s\n", ofile);
	}else
		write(1, out, nout);
	exits(nil);
}
