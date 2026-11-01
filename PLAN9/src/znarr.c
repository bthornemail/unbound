/*
 * znarr - Metaverse Kit's narrative world compiler (~/devops/metaverse-kit
 * tools/mv-narrative-world-compile/index.py) in Plan 9 C, literal (the author's choice,
 * as zcheck, ztime and zshard): no digests. The tool stamps the world and the manifest
 * with a sha256 of themselves and names the source by its sha256; here the world names
 * its source by its path, the manifest keeps the source and the counts, and validate
 * recompiles the source and compares the four files byte for byte, the world being the
 * compile of its source.
 *	znarr emit in.yaml outdir	narrative-world.v0.json, .asg.json, .timeline.ndjson,
 *					.manifest.json; ok and the counts, or ERROR: why (exit 1)
 *	znarr validate outdir		ok, or ERROR: missing output, or ERROR: NAME literal mismatch
 *	znarr -n validate outdir	the falsifier: the four files' presence only, not their bytes
 * The YAML read is the narratives': block mappings and lists, flow lists and mappings,
 * double- and single-quoted and plain scalars, with PyYAML's YAML 1.1 resolution of a plain
 * scalar where the compiler writes str() of it (null, bool, int: decimal, 0b, 0 octal, 0x,
 * underscores; float as Python's repr). Outside the claim: anchors, tags, multi-line
 * scalars, dates, sexagesimal numbers, .inf and .nan, a key that is not a string.
 */
#include <u.h>
#include <libc.h>

enum { YMap, YSeq, YStr };
typedef struct Y Y;
struct Y {
	int	t;
	char	*s;	/* YStr: the text, decoded if quoted */
	int	quoted;
	Y	**k, **v;
	int	n;
};

typedef struct Buf Buf;
struct Buf { char *s; long n, cap; };

static void
bput(Buf *b, char *s)
{
	long m;

	m = strlen(s);
	if(b->n + m + 1 > b->cap){
		b->cap = (b->n + m + 1) * 2;
		b->s = realloc(b->s, b->cap);
	}
	memmove(b->s + b->n, s, m + 1);
	b->n += m;
}

static void
bputc(Buf *b, int c)
{
	char s[2];

	s[0] = c; s[1] = 0;
	bput(b, s);
}

static char *
bstr(Buf *b)
{
	if(b->s == nil)
		bput(b, "");
	return b->s;
}

static void
die(char *msg)
{
	fprint(2, "ERROR: %s\n", msg);
	exits("1");
}

static char*
slurp(char *file)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		return nil;
	s = mallocz(d->length + 1, 1);
	readn(fd, s, d->length);
	close(fd);
	free(d);
	return s;
}

static Y*
mk(int t)
{
	Y *y;

	y = mallocz(sizeof *y, 1);
	y->t = t;
	return y;
}

static void
add(Y *m, Y *k, Y *v)
{
	m->k = realloc(m->k, (m->n+1) * sizeof(Y*));
	m->v = realloc(m->v, (m->n+1) * sizeof(Y*));
	m->k[m->n] = k;
	m->v[m->n++] = v;
}

static Y*
scalar(char *s, int quoted)
{
	Y *y;

	y = mk(YStr);
	y->s = s;
	y->quoted = quoted;
	return y;
}

/* ── YAML 1.1 resolution of a plain scalar, written as Python's str() ── */

static int
match(char *s, char *set)	/* s non-empty, every byte in set */
{
	if(*s == 0)
		return 0;
	for(; *s; s++)
		if(strchr(set, *s) == nil)
			return 0;
	return 1;
}

static char*
undersc(char *s)
{
	char *o, *p;

	o = p = strdup(s);
	for(; *s; s++)
		if(*s != '_')
			*p++ = *s;
	*p = 0;
	return o;
}

/* Python's repr of a float: the shortest digits that read back, fixed between 1e-4 and 1e16 */
static char*
pyfloat(double v)
{
	char buf[64], dig[32], *e, *p;
	int prec, x, nd, i;
	Buf b;

	if(v == 0)
		return strdup(1/v < 0 ? "-0.0" : "0.0");
	for(prec = 1; prec <= 17; prec++){
		snprint(buf, sizeof buf, "%.*e", prec - 1, v);
		if(strtod(buf, nil) == v)
			break;
	}
	memset(&b, 0, sizeof b);
	p = buf;
	if(*p == '-'){
		bputc(&b, '-');
		p++;
	}
	nd = 0;
	for(; *p && *p != 'e' && *p != 'E'; p++)
		if(*p >= '0' && *p <= '9')
			dig[nd++] = *p;
	dig[nd] = 0;
	while(nd > 1 && dig[nd-1] == '0')
		dig[--nd] = 0;
	e = p;
	x = (*e == 'e' || *e == 'E') ? atoi(e + 1) : 0;
	if(x >= -4 && x < 16){
		if(x >= 0){
			for(i = 0; i <= x; i++)
				bputc(&b, i < nd ? dig[i] : '0');
			bputc(&b, '.');
			if(nd > x + 1)
				bput(&b, dig + x + 1);
			else
				bputc(&b, '0');
		}else{
			bput(&b, "0.");
			for(i = 0; i < -x - 1; i++)
				bputc(&b, '0');
			bput(&b, dig);
		}
	}else{
		bputc(&b, dig[0]);
		if(nd > 1){
			bputc(&b, '.');
			bput(&b, dig + 1);
		}
		snprint(buf, sizeof buf, "e%c%.2d", x < 0 ? '-' : '+', x < 0 ? -x : x);
		bput(&b, buf);
	}
	return bstr(&b);
}

static char*
pystr(Y *y)
{
	char *s, *t, *d;
	int neg;
	vlong v;

	if(y == nil)
		return "None";
	if(y->t != YStr)
		return "";	/* str() of a mapping or list: outside the claim */
	s = y->s;
	if(y->quoted)
		return s;
	if(*s == 0 || strcmp(s, "~") == 0 || strcmp(s, "null") == 0 || strcmp(s, "Null") == 0 || strcmp(s, "NULL") == 0)
		return "None";
	if(strcmp(s, "yes") == 0 || strcmp(s, "Yes") == 0 || strcmp(s, "YES") == 0 || strcmp(s, "true") == 0 || strcmp(s, "True") == 0 || strcmp(s, "TRUE") == 0 || strcmp(s, "on") == 0 || strcmp(s, "On") == 0 || strcmp(s, "ON") == 0)
		return "True";
	if(strcmp(s, "no") == 0 || strcmp(s, "No") == 0 || strcmp(s, "NO") == 0 || strcmp(s, "false") == 0 || strcmp(s, "False") == 0 || strcmp(s, "FALSE") == 0 || strcmp(s, "off") == 0 || strcmp(s, "Off") == 0 || strcmp(s, "OFF") == 0)
		return "False";
	t = s;
	neg = 0;
	if(*t == '-' || *t == '+'){
		neg = *t == '-';
		t++;
	}
	v = -1;
	if(t[0] == '0' && t[1] == 'b' && match(t + 2, "01_"))
		v = strtoll(undersc(t + 2), nil, 2);
	else if(t[0] == '0' && t[1] == 'x' && match(t + 2, "0123456789abcdefABCDEF_"))
		v = strtoll(undersc(t + 2), nil, 16);
	else if(t[0] == '0' && match(t + 1, "01234567_"))
		v = strtoll(undersc(t + 1), nil, 8);
	else if(strcmp(t, "0") == 0 || (t[0] >= '1' && t[0] <= '9' && match(t, "0123456789_")))
		v = strtoll(undersc(t), nil, 10);
	if(v >= 0)
		return smprint("%lld", neg ? -v : v);
	/* float: [-+]?[0-9][0-9_]*\.[0-9_]*([eE][-+][0-9]+)? or \.[0-9_]+([eE][-+][0-9]+)? */
	d = t;
	if(*d >= '0' && *d <= '9'){
		while((*d >= '0' && *d <= '9') || *d == '_')
			d++;
		if(*d != '.')
			return s;
	}else if(*d != '.' || !(d[1] >= '0' && d[1] <= '9') || s != t)
		return s;
	d++;
	while((*d >= '0' && *d <= '9') || *d == '_')
		d++;
	if(*d == 'e' || *d == 'E'){
		d++;
		if(*d != '-' && *d != '+')
			return s;
		d++;
		if(!(*d >= '0' && *d <= '9'))
			return s;
		while(*d >= '0' && *d <= '9')
			d++;
	}
	if(*d != 0)
		return s;
	return pyfloat(strtod(undersc(s), nil));
}

/* ── the YAML reader: lines, indentation, flow ───────────────────── */

typedef struct Line Line;
struct Line { int ind; char *s; };
static Line *L;
static int nl;

static void
lines(char *text)
{
	char *p, *e;
	int i;

	for(p = text; *p; p = e){
		e = strchr(p, '\n');
		e = e ? e + 1 : p + strlen(p);
		for(i = 0; p + i < e && p[i] == ' '; i++)
			;
		if(p + i == e || p[i] == '\n' || p[i] == '\r' || p[i] == '#')
			continue;
		if(strncmp(p, "---", 3) == 0 && (p[3] == '\n' || p[3] == 0 || p[3] == ' ' || p[3] == '\r'))
			continue;
		L = realloc(L, (nl+1) * sizeof(Line));
		L[nl].ind = i;
		L[nl].s = smprint("%.*s", (int)(e - p - i), p + i);
		while(*L[nl].s && (L[nl].s[strlen(L[nl].s)-1] == '\n' || L[nl].s[strlen(L[nl].s)-1] == '\r' || L[nl].s[strlen(L[nl].s)-1] == ' '))
			L[nl].s[strlen(L[nl].s)-1] = 0;
		nl++;
	}
}

static int
hexval(int c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;
}

/* a quoted scalar from *pp, its quote first; *pp left after the closing quote */
static char*
quoted(char **pp)
{
	char *p, ch[UTFmax+1];
	int q, i, n;
	Rune r;
	Buf b;

	memset(&b, 0, sizeof b);
	p = *pp;
	q = *p++;
	for(; *p; p++){
		if(*p == q){
			if(q == '\'' && p[1] == '\''){
				bputc(&b, '\'');
				p++;
				continue;
			}
			p++;
			break;
		}
		if(q == '"' && *p == '\\' && p[1]){
			p++;
			switch(*p){
			case 'n': bputc(&b, '\n'); break;
			case 't': bputc(&b, '\t'); break;
			case 'r': bputc(&b, '\r'); break;
			case 'b': bputc(&b, '\b'); break;
			case 'f': bputc(&b, '\f'); break;
			case 'v': bputc(&b, '\v'); break;
			case '0': break;	/* a NUL: outside the claim */
			case 'a': bputc(&b, 7); break;
			case 'e': bputc(&b, 27); break;
			case 'x': case 'u': case 'U':
				n = *p == 'x' ? 2 : *p == 'u' ? 4 : 8;
				for(r = 0, i = 0; i < n && p[1]; i++)
					r = r * 16 + hexval(*++p);
				ch[runetochar(ch, &r)] = 0;
				bput(&b, ch);
				break;
			default: bputc(&b, *p);
			}
			continue;
		}
		bputc(&b, *p);
	}
	*pp = p;
	return bstr(&b);
}

static Y *flow(char **pp);

static char*
skipws(char *p)
{
	while(*p == ' ' || *p == '\t')
		p++;
	return p;
}

/* a plain scalar in a flow: up to , ] } or a ": " */
static char*
flowplain(char **pp)
{
	char *p, *e;

	p = *pp;
	for(e = p; *e && *e != ',' && *e != ']' && *e != '}' && !(*e == ':' && (e[1] == ' ' || e[1] == ',' || e[1] == '}' || e[1] == ']' || e[1] == 0)); e++)
		;
	*pp = e;
	while(e > p && (e[-1] == ' ' || e[-1] == '\t'))
		e--;
	return smprint("%.*s", (int)(e - p), p);
}

static Y*
flow(char **pp)
{
	char *p;
	Y *y, *k;

	p = skipws(*pp);
	if(*p == '['){
		y = mk(YSeq);
		p = skipws(p + 1);
		while(*p && *p != ']'){
			add(y, nil, flow(&p));
			p = skipws(p);
			if(*p == ',')
				p = skipws(p + 1);
		}
		*pp = *p ? p + 1 : p;
		return y;
	}
	if(*p == '{'){
		y = mk(YMap);
		p = skipws(p + 1);
		while(*p && *p != '}'){
			k = (*p == '"' || *p == '\'') ? scalar(quoted(&p), 1) : scalar(flowplain(&p), 0);
			p = skipws(p);
			if(*p == ':')
				p = skipws(p + 1);
			add(y, k, (*p == ',' || *p == '}') ? scalar("", 0) : flow(&p));
			p = skipws(p);
			if(*p == ',')
				p = skipws(p + 1);
		}
		*pp = *p ? p + 1 : p;
		return y;
	}
	if(*p == '"' || *p == '\''){
		y = scalar(quoted(&p), 1);
		*pp = p;
		return y;
	}
	y = scalar(flowplain(&p), 0);
	*pp = p;
	return y;
}

/* a value on its line: flow, quoted, or plain to the end (a " #" comment dropped) */
static Y*
inline1(char *s)
{
	char *p, *c;

	p = skipws(s);
	if(*p == '[' || *p == '{' || *p == '"' || *p == '\'')
		return flow(&p);
	p = strdup(p);
	if((c = strstr(p, " #")) != nil)
		*c = 0;
	while(*p && (p[strlen(p)-1] == ' ' || p[strlen(p)-1] == '\t'))
		p[strlen(p)-1] = 0;
	return scalar(p, 0);
}

/* the position of a mapping key's colon: a ':' followed by a space or the end; -1 if none */
static int
keycolon(char *s)
{
	char *p;

	if(*s == '"' || *s == '\''){
		p = s;
		quoted(&p);
		p = skipws(p);
		return *p == ':' ? p - s : -1;
	}
	if(*s == '[' || *s == '{' || *s == '-' && (s[1] == ' ' || s[1] == 0))
		return -1;
	for(p = s; *p; p++)
		if(*p == ':' && (p[1] == ' ' || p[1] == 0))
			return p - s;
		else if(*p == ' ' && p[1] == '#')
			return -1;
	return -1;
}

static int isitem(char *s) { return s[0] == '-' && (s[1] == ' ' || s[1] == 0); }

static Y *block(int *i, int ind);

/* the value after "key:" or "-": inline, or the block below, or null */
static Y*
below(int *i, int ind, char *rest)
{
	if(*skipws(rest))
		return (*i)++, inline1(rest);
	(*i)++;
	if(*i < nl && (L[*i].ind > ind || (L[*i].ind == ind && isitem(L[*i].s))))
		return block(i, L[*i].ind);
	return scalar("", 0);
}

static Y*
block(int *i, int ind)
{
	Y *y, *k;
	char *s, *p;
	int c, col;

	if(isitem(L[*i].s)){
		y = mk(YSeq);
		while(*i < nl && L[*i].ind == ind && isitem(L[*i].s)){
			s = L[*i].s + 1;
			for(col = 1; s[0] == ' '; s++)
				col++;
			if(*s && keycolon(s) >= 0){	/* "- key: value": a mapping from this column */
				L[*i].ind = ind + col;
				L[*i].s = s;
				add(y, nil, block(i, ind + col));
			}else
				add(y, nil, below(i, ind, s));
		}
		return y;
	}
	y = mk(YMap);
	while(*i < nl && L[*i].ind == ind && !isitem(L[*i].s)){
		s = L[*i].s;
		if((c = keycolon(s)) < 0)
			die("yaml: a line that is neither a key nor an item");
		if(*s == '"' || *s == '\''){
			p = s;
			k = scalar(quoted(&p), 1);
		}else
			k = scalar(smprint("%.*s", c, s), 0);
		add(y, k, below(i, ind, s + c + 1));
	}
	return y;
}

static Y*
load(char *text)
{
	int i;

	lines(text);
	if(nl == 0)
		return scalar("", 0);
	if(nl == 1 && keycolon(L[0].s) < 0 && !isitem(L[0].s))
		return inline1(L[0].s);
	i = 0;
	return block(&i, L[0].ind);
}

/* the value at a key, keys compared as their str() */
static Y*
get(Y *m, char *key)
{
	int i;

	if(m == nil || m->t != YMap)
		return nil;
	for(i = m->n - 1; i >= 0; i--)	/* the last of a repeated key, as PyYAML keeps it */
		if(strcmp(pystr(m->k[i]), key) == 0)
			return m->v[i];
	return nil;
}
static int has(Y *m, char *key) { int i; if(m == nil || m->t != YMap) return 0; for(i = 0; i < m->n; i++) if(strcmp(pystr(m->k[i]), key) == 0) return 1; return 0; }
static int isnone(Y *y) { return y == nil || (y->t == YStr && strcmp(pystr(y), "None") == 0); }

/* ── JSON, as json.dumps(..., ensure_ascii=False, separators=(",", ":")) ── */

static void
jstr(Buf *b, char *s)
{
	char u[8];

	bputc(b, '"');
	for(; *s; s++){
		switch(*s){
		case '"': bput(b, "\\\""); break;
		case '\\': bput(b, "\\\\"); break;
		case '\n': bput(b, "\\n"); break;
		case '\r': bput(b, "\\r"); break;
		case '\t': bput(b, "\\t"); break;
		case '\b': bput(b, "\\b"); break;
		case '\f': bput(b, "\\f"); break;
		default:
			if((uchar)*s < 0x20){
				snprint(u, sizeof u, "\\u%04x", (uchar)*s);
				bput(b, u);
			}else
				bputc(b, *s);
		}
	}
	bputc(b, '"');
}

static void jkv(Buf *b, char *k, char *v, int last) { jstr(b, k); bputc(b, ':'); jstr(b, v); if(!last) bputc(b, ','); }

/* ── the compiler ──────────────────────────────────────────────────── */

typedef struct Scene Scene;
struct Scene { char *key, *id, *title, *loc, *time, *atm, *next; char **cp; int ncp; long sk; };
typedef struct Node Node;
struct Node { char *id, *label, *type; int ord; };
typedef struct Edge Edge;
struct Edge { char *from, *to, *kind; };

static int
scenecmp(void *a, void *b)
{
	Scene *x, *y;

	x = a; y = b;
	if(x->sk != y->sk)
		return x->sk < y->sk ? -1 : 1;
	return strcmp(x->key, y->key);
}

static int
nodecmp(void *a, void *b)
{
	Node *x, *y;
	int c;

	x = a; y = b;
	if((c = strcmp(x->id, y->id)) != 0)
		return c;
	return x->ord - y->ord;
}

static int
edgecmp(void *a, void *b)
{
	Edge *x, *y;
	int c;

	x = a; y = b;
	if((c = strcmp(x->from, y->from)) != 0) return c;
	if((c = strcmp(x->to, y->to)) != 0) return c;
	return strcmp(x->kind, y->kind);
}

static int
keycmp(void *a, void *b)	/* sorted(d.items()): by the keys' str() */
{
	return strcmp(pystr(*(Y**)a), pystr(*(Y**)b));
}

/* the indices of a mapping's entries sorted by key */
static int*
sortedkeys(Y *m)
{
	Y **ks;
	int *ix, i, j;

	ks = malloc((m->n + 1) * sizeof(Y*));
	memmove(ks, m->k, m->n * sizeof(Y*));
	qsort(ks, m->n, sizeof(Y*), keycmp);
	ix = malloc((m->n + 1) * sizeof(int));
	for(i = 0; i < m->n; i++)
		for(j = 0; j < m->n; j++)
			if(m->k[j] == ks[i]){
				ix[i] = j;
				break;
			}
	return ix;
}

static char *source;
static char *files[4];
static long nscenes, nnodes, nedges, ntimeline;

static char*
pylist(char **v, int n)
{
	Buf b;
	int i;

	memset(&b, 0, sizeof b);
	bputc(&b, '[');
	for(i = 0; i < n; i++)
		bput(&b, smprint("%s'%s'", i ? ", " : "", v[i]));
	bputc(&b, ']');
	return bstr(&b);
}

static void
compile(char *text, char *srcpath)
{
	static char *top[] = { "narrative", "characters", "settings", "objects", "scenes", "transitions", "relationships", "thematic_clusters", "narrative_metadata", "trace_system" };
	static char *req[] = { "id", "title", "location", "time", "characters_present", "next_scene" };
	static char *head[] = { "id", "title", "version", "timestamp" };
	char *missing[10], *s, *suf;
	Y *doc, *nar, *chars, *scenes, *sets, *objs, *row, *cps, *c, *sv;
	Scene *sc;
	Node *nd;
	Edge *ed;
	int nm, i, j, k, ns, nn, ne, *ix, tl, ncount;
	Buf w, a, t, m;
	char **seen;

	doc = load(text);
	if(doc->t != YMap)
		die("root yaml must be object");
	for(nm = 0, i = 0; i < nelem(top); i++)
		if(!has(doc, top[i]))
			missing[nm++] = top[i];
	if(nm)
		die(smprint("missing top-level keys: %s", pylist(missing, nm)));
	nar = get(doc, "narrative");
	chars = get(doc, "characters");
	scenes = get(doc, "scenes");
	sets = get(doc, "settings");
	objs = get(doc, "objects");
	if(nar == nil || nar->t != YMap)
		die("narrative must be object");
	for(i = 0; i < nelem(head); i++)
		if(!has(nar, head[i]))
			die(smprint("narrative.%s missing", head[i]));
	if(chars == nil || chars->t != YMap || chars->n == 0)
		die("characters must be non-empty object");
	if(scenes == nil || scenes->t != YMap || scenes->n == 0)
		die("scenes must be non-empty object");

	sc = malloc(scenes->n * sizeof(Scene));
	for(i = 0; i < scenes->n; i++){
		sc[i].key = pystr(scenes->k[i]);
		sc[i].sk = 10000;
		if(strncmp(sc[i].key, "scene_", 6) == 0){
			suf = sc[i].key + 6;
			if(match(suf, "0123456789"))
				sc[i].sk = strtol(suf, nil, 10);
		}
		sc[i].ncp = i;	/* the entry, for now */
	}
	qsort(sc, scenes->n, sizeof(Scene), scenecmp);
	ns = scenes->n;
	seen = malloc((ns + 1) * sizeof(char*));
	for(i = 0; i < ns; i++){
		row = scenes->v[sc[i].ncp];
		if(row->t != YMap)
			die(smprint("scenes.%s must be object", sc[i].key));
		for(j = 0; j < nelem(req); j++)
			if(!has(row, req[j]))
				die(smprint("scenes.%s.%s missing", sc[i].key, req[j]));
		sc[i].id = pystr(get(row, "id"));
		for(j = 0; j < i; j++)
			if(strcmp(seen[j], sc[i].id) == 0)
				die(smprint("duplicate scene id: %s", sc[i].id));
		seen[i] = sc[i].id;
		cps = get(row, "characters_present");
		if(cps == nil || cps->t != YSeq)
			die(smprint("scenes.%s.characters_present must be list", sc[i].key));
		sc[i].title = pystr(get(row, "title"));
		sc[i].loc = pystr(get(row, "location"));
		sc[i].time = pystr(get(row, "time"));
		sc[i].atm = has(row, "atmosphere") ? pystr(get(row, "atmosphere")) : "";
		sc[i].next = pystr(get(row, "next_scene"));
		sc[i].cp = malloc((cps->n + 1) * sizeof(char*));
		for(j = 0; j < cps->n; j++)
			sc[i].cp[j] = pystr(cps->v[j]);
		sc[i].ncp = cps->n;
	}

	/* the nodes: scenes, characters, settings, objects, sorted by id, in that order when equal */
	nd = malloc((ns + chars->n + (sets && sets->t == YMap ? sets->n : 0) + (objs && objs->t == YMap ? objs->n : 0) + 1) * sizeof(Node));
	nn = 0;
	for(i = 0; i < ns; i++, nn++){
		nd[nn].id = smprint("scene:%s", sc[i].id); nd[nn].label = sc[i].title; nd[nn].type = "scene"; nd[nn].ord = nn;
	}
	ix = sortedkeys(chars);
	ncount = 0;
	for(i = 0; i < chars->n; i++){
		c = chars->v[ix[i]];
		if(c->t != YMap || !has(c, "id"))
			die(smprint("characters.%s.id missing", pystr(chars->k[ix[i]])));
		nd[nn].id = smprint("char:%s", pystr(get(c, "id"))); nd[nn].label = pystr(chars->k[ix[i]]); nd[nn].type = "character"; nd[nn].ord = nn;
		nn++; ncount++;
	}
	for(k = 0; k < 2; k++){
		Y *g = k == 0 ? sets : objs;
		if(g == nil || g->t != YMap)
			continue;
		ix = sortedkeys(g);
		for(i = 0; i < g->n; i++){
			sv = g->v[ix[i]];
			if(sv->t == YMap && has(sv, "id")){
				nd[nn].id = smprint(k == 0 ? "setting:%s" : "object:%s", pystr(get(sv, "id")));
				nd[nn].label = pystr(g->k[ix[i]]); nd[nn].type = k == 0 ? "setting" : "object"; nd[nn].ord = nn;
				nn++;
			}
		}
	}
	qsort(nd, nn, sizeof(Node), nodecmp);

	/* the edges: transitions, characters present, the setting of the location */
	ed = nil;
	ne = 0;
	for(i = 0; i < ns; i++){
		for(j = 0; j < ns; j++)
			if(strcmp(sc[j].id, sc[i].next) == 0){
				ed = realloc(ed, (ne+1) * sizeof(Edge));
				ed[ne].from = smprint("scene:%s", sc[i].id); ed[ne].to = smprint("scene:%s", sc[i].next); ed[ne++].kind = "transitions_to";
				break;
			}
		for(j = 0; j < sc[i].ncp; j++)
			for(k = chars->n - 1; k >= 0; k--)	/* the last of a repeated name, as a dict keeps it */
				if(strcmp(pystr(chars->k[k]), sc[i].cp[j]) == 0){
					ed = realloc(ed, (ne+1) * sizeof(Edge));
					ed[ne].from = smprint("scene:%s", sc[i].id); ed[ne].to = smprint("char:%s", pystr(get(chars->v[k], "id"))); ed[ne++].kind = "contains_character";
					break;
				}
		s = strstr(sc[i].loc, "::") ? smprint("%.*s", (int)(strstr(sc[i].loc, "::") - sc[i].loc), sc[i].loc) : sc[i].loc;
		sv = sets != nil && sets->t == YMap ? get(sets, s) : nil;
		if(sv != nil && sv->t == YMap && has(sv, "id")){
			ed = realloc(ed, (ne+1) * sizeof(Edge));
			ed[ne].from = smprint("scene:%s", sc[i].id); ed[ne].to = smprint("setting:%s", pystr(get(sv, "id"))); ed[ne++].kind = "located_in";
		}
	}
	qsort(ed, ne, sizeof(Edge), edgecmp);
	for(i = 0, j = 0; i < ne; i++)
		if(j == 0 || edgecmp(&ed[i], &ed[j-1]) != 0)
			ed[j++] = ed[i];
	ne = j;

	/* the graph, once, for the world and the asg */
	memset(&a, 0, sizeof a);
	bput(&a, "\"edges\":[");
	for(i = 0; i < ne; i++){
		bput(&a, i ? ",{" : "{");
		jkv(&a, "from", ed[i].from, 0); jkv(&a, "kind", ed[i].kind, 0); jkv(&a, "to", ed[i].to, 1);
		bputc(&a, '}');
	}
	bput(&a, "],\"nodes\":[");
	for(i = 0; i < nn; i++){
		bput(&a, i ? ",{" : "{");
		jkv(&a, "id", nd[i].id, 0); jkv(&a, "label", nd[i].label, 0); jkv(&a, "type", nd[i].type, 1);
		bputc(&a, '}');
	}
	bputc(&a, ']');

	memset(&w, 0, sizeof w);
	bput(&w, "{");
	jkv(&w, "authority", "advisory", 0);
	bput(&w, "\"graph\":{"); bput(&w, bstr(&a)); bput(&w, "},");
	jkv(&w, "narrative_id", pystr(get(nar, "id")), 0);
	bput(&w, "\"scenes\":[");
	for(i = 0; i < ns; i++){
		bput(&w, i ? ",{" : "{");
		jkv(&w, "atmosphere", sc[i].atm, 0); jkv(&w, "character_count", smprint("%d", sc[i].ncp), 0); jkv(&w, "location", sc[i].loc, 0);
		jkv(&w, "next_scene", sc[i].next, 0); jkv(&w, "scene_id", sc[i].id, 0); jkv(&w, "time", sc[i].time, 0); jkv(&w, "title", sc[i].title, 1);
		bputc(&w, '}');
	}
	bput(&w, "],");
	jkv(&w, "source_path", srcpath, 0);
	bput(&w, "\"summary\":{");
	jkv(&w, "character_count", smprint("%d", ncount), 0); jkv(&w, "edge_count", smprint("%d", ne), 0);
	jkv(&w, "node_count", smprint("%d", nn), 0); jkv(&w, "scene_count", smprint("%d", ns), 1);
	bput(&w, "},");
	jkv(&w, "title", pystr(get(nar, "title")), 0);
	jkv(&w, "v", "narrative.world.v0", 1);
	bput(&w, "}\n");

	/* the timeline: from the first scene, next by next, until a scene comes again or none */
	memset(&t, 0, sizeof t);
	bput(&t, "");
	tl = 0;
	for(i = 0; i < ns; i++)
		seen[i] = nil;
	for(i = 0; i >= 0 && ns > 0 && seen[i] == nil; ){
		seen[i] = "x";
		bputc(&t, '{');
		jkv(&t, "authority", "advisory", 0); jkv(&t, "event", "scene_enter", 0); jkv(&t, "location", sc[i].loc, 0);
		jkv(&t, "next_scene", sc[i].next, 0); jkv(&t, "scene_id", sc[i].id, 0); jkv(&t, "t", smprint("%d", tl), 0); jkv(&t, "title", sc[i].title, 1);
		bput(&t, "}\n");
		tl++;
		for(j = 0, k = -1; j < ns; j++)
			if(strcmp(sc[j].id, sc[i].next) == 0){
				k = j;
				break;
			}
		i = k;
	}

	memset(&m, 0, sizeof m);
	bput(&m, "{");
	jkv(&m, "authority", "advisory", 0);
	bput(&m, "\"counts\":{");
	jkv(&m, "edge_count", smprint("%d", ne), 0); jkv(&m, "node_count", smprint("%d", nn), 0);
	jkv(&m, "scene_count", smprint("%d", ns), 0); jkv(&m, "timeline_events", smprint("%d", tl), 1);
	bput(&m, "},");
	jkv(&m, "source_path", srcpath, 0);
	jkv(&m, "v", "narrative.world.manifest.v0", 1);
	bput(&m, "}\n");

	files[0] = bstr(&w);
	files[1] = smprint("{%s}\n", bstr(&a));
	files[2] = bstr(&t);
	files[3] = bstr(&m);
	nscenes = ns; nnodes = nn; nedges = ne; ntimeline = tl;
}

static char *name[] = { "narrative-world.v0.json", "narrative-world.asg.json", "narrative-world.timeline.ndjson", "narrative-world.manifest.json" };
static char *what[] = { "world", "asg", "timeline", "manifest" };

static char*
absolute(char *p)
{
	char wd[512];

	if(p[0] == '/')
		return p;
	if(getwd(wd, sizeof wd) == nil)
		sysfatal("getwd: %r");
	while(p[0] == '.' && p[1] == '/')
		p += 2;
	return smprint("%s/%s", wd, p);
}

static void
mkdirp(char *d)
{
	char *s, *q;
	int c;

	s = strdup(d);
	for(q = s; ; q++){
		if((*q == '/' || *q == 0) && q > s){
			c = *q;
			*q = 0;
			if(access(s, AEXIST) < 0)
				close(create(s, OREAD, DMDIR|0777));
			*q = c;
		}
		if(*q == 0)
			break;
	}
	free(s);
}

static void
emit(char *in, char *out)
{
	char *text;
	int i, fd;

	source = absolute(in);
	if((text = slurp(in)) == nil)
		sysfatal("open %s: %r", in);
	compile(text, source);
	out = absolute(out);
	mkdirp(out);
	for(i = 0; i < 4; i++){
		if((fd = create(smprint("%s/%s", out, name[i]), OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", name[i]);
		write(fd, files[i], strlen(files[i]));
		close(fd);
	}
	print("ok mv-narrative-world-compile emit scenes=%ld nodes=%ld edges=%ld timeline=%ld\n", nscenes, nnodes, nedges, ntimeline);
	exits(nil);
}

/* the world's source_path, read back from its JSON */
static char*
sourceof(char *world)
{
	char *p;
	Buf b;

	if((p = strstr(world, "\"source_path\":\"")) == nil)
		return nil;
	memset(&b, 0, sizeof b);
	for(p += 15; *p && *p != '"'; p++){
		if(*p == '\\' && p[1]){
			p++;
			switch(*p){
			case 'n': bputc(&b, '\n'); break;
			case 't': bputc(&b, '\t'); break;
			case 'r': bputc(&b, '\r'); break;
			default: bputc(&b, *p);
			}
		}else
			bputc(&b, *p);
	}
	return bstr(&b);
}

static void
validate(char *out, int check)
{
	char *got[4], *src, *text;
	int i;

	out = absolute(out);
	for(i = 0; i < 4; i++)
		if((got[i] = slurp(smprint("%s/%s", out, name[i]))) == nil)
			die(smprint("missing output: %s/%s", out, name[i]));
	if(check){
		if((src = sourceof(got[0])) == nil || (text = slurp(src)) == nil)
			die(smprint("missing source: %s", src ? src : "?"));
		compile(text, src);
		for(i = 0; i < 4; i++)
			if(strcmp(got[i], files[i]) != 0)
				die(smprint("%s literal mismatch", what[i]));
	}
	print("ok mv-narrative-world-compile validate\n");
	exits(nil);
}

void
main(int argc, char *argv[])
{
	int check;

	check = 1;
	ARGBEGIN{
	case 'n':
		check = 0;
		break;
	}ARGEND
	if(argc == 3 && strcmp(argv[0], "emit") == 0)
		emit(argv[1], argv[2]);
	if(argc == 2 && strcmp(argv[0], "validate") == 0)
		validate(argv[1], check);
	sysfatal("usage: znarr [-n] emit in.yaml outdir | validate outdir");
}
