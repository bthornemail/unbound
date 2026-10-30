/*
 * zshard - metaverse-build's shards (~/devops/metaverse-build runtime/shards) in Plan 9 C,
 * literal, as zcheck and ztime: where bundle.py's manifest keeps each file's sha256 and
 * restore.py compares sha256s, the manifest here keeps each file's bytes as a JSON string
 * and restore compares bytes; the manifest is then as large as what it carries.
 *	zshard bundle zone snapshot trace checkpoint outdir	bundle.py: OUTDIR/ZONE/ with the
 *						three files and manifest.json; prints the manifest's path
 *	zshard restore bundledir		restore.py: OK, or HALT: snapshot|trace|checkpoint
 *						literal mismatch (3), where the tool says "hash"
 *	zshard fields manifest			zone|snapshot|trace|checkpoint
 *	zshard -n restore ...			the falsifier: the literals not compared
 * A malformed input writes its reason and exits "2". A file holding a NUL byte is outside
 * the port's claim.
 */
#include <u.h>
#include <libc.h>

enum { Null, True, False, Num, Str, Arr, Obj };

typedef struct J J;
struct J {
	int	t;
	char	*s;
	J	**v;
	char	**k;
	int	n;
};

static char *p;
static J*
mk(int t)
{
	J *j;

	j = mallocz(sizeof *j, 1);
	j->t = t;
	return j;
}

static void
ws(void)
{
	while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
}

static char*
lit(void)
{
	char *b;

	b = p++;
	while(*p && *p != '"'){
		if(*p == '\\' && p[1])
			p++;
		p++;
	}
	p++;
	return smprint("%.*s", (int)(p - b), b);
}

static void
add(J *j, char *k, J *v)
{
	j->v = realloc(j->v, (j->n+1) * sizeof(J*));
	j->k = realloc(j->k, (j->n+1) * sizeof(char*));
	j->k[j->n] = k;
	j->v[j->n++] = v;
}

static J*
val(void)
{
	J *j;
	char *b, *k;

	ws();
	switch(*p){
	case '{':
		j = mk(Obj);
		p++; ws();
		if(*p == '}'){ p++; return j; }
		for(;;){
			ws(); k = lit(); ws(); p++;
			add(j, k, val());
			ws();
			if(*p++ == '}')
				return j;
		}
	case '[':
		j = mk(Arr);
		p++; ws();
		if(*p == ']'){ p++; return j; }
		for(;;){
			add(j, nil, val());
			ws();
			if(*p++ == ']')
				return j;
		}
	case '"':
		j = mk(Str);
		j->s = lit();
		return j;
	case 't': p += 4; return mk(True);
	case 'f': p += 5; return mk(False);
	case 'n': p += 4; return mk(Null);
	}
	j = mk(Num);
	b = p;
	while(*p && strchr("-+.0123456789eE", *p))
		p++;
	j->s = smprint("%.*s", (int)(p - b), b);
	return j;
}

static J*
get(J *o, char *k)
{
	int i, n;

	if(o == nil || o->t != Obj)
		return nil;
	n = strlen(k);
	for(i = 0; i < o->n; i++)
		if(strlen(o->k[i]) == n+2 && memcmp(o->k[i]+1, k, n) == 0)
			return o->v[i];
	return nil;
}

static void
put(J *o, char *k, J *v)
{
	add(o, smprint("\"%s\"", k), v);
}

static char *bare(J *j) { return smprint("%.*s", (int)strlen(j->s)-2, j->s+1); }

static J*
str(char *s)
{
	J *j;

	j = mk(Str);
	j->s = smprint("\"%s\"", s);
	return j;
}

static int
keycmp(void *a, void *b)
{
	char *x, *y;
	int m, n, c;

	x = *(char**)a; y = *(char**)b;
	m = strlen(x) - 2; n = strlen(y) - 2;
	c = memcmp(x+1, y+1, m < n ? m : n);
	return c ? c : m - n;
}

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
	memmove(b->s + b->n, s, m+1);
	b->n += m;
}

static void
canon(Buf *b, J *j)
{
	char **ks;
	int i, h;

	switch(j->t){
	case Null: bput(b, "null"); return;
	case True: bput(b, "true"); return;
	case False: bput(b, "false"); return;
	case Num: case Str: bput(b, j->s); return;
	case Arr:
		bput(b, "[");
		for(i = 0; i < j->n; i++){
			if(i) bput(b, ",");
			canon(b, j->v[i]);
		}
		bput(b, "]");
		return;
	}
	ks = malloc((j->n+1) * sizeof(char*));
	memmove(ks, j->k, j->n * sizeof(char*));
	qsort(ks, j->n, sizeof(char*), keycmp);
	bput(b, "{");
	for(i = 0; i < j->n; i++){
		if(i) bput(b, ",");
		for(h = 0; j->k[h] != ks[i]; h++)
			;
		bput(b, ks[i]);
		bput(b, ":");
		canon(b, j->v[h]);
	}
	bput(b, "}");
	free(ks);
}

static char*
canonical(J *j)
{
	Buf b;

	memset(&b, 0, sizeof b);
	bput(&b, "");
	canon(&b, j);
	return b.s;
}

static void
fail(char *why)
{
	fprint(2, "%s\n", why);
	exits("2");
}

static char*
slurp(char *file)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", file);
	s = mallocz(d->length + 1, 1);
	if(readn(fd, s, d->length) != d->length)
		sysfatal("read %s: %r", file);
	close(fd);
	free(d);
	return s;
}

static J*
load(char *file)
{
	p = slurp(file);
	return val();
}

static void
spit(char *file, char *s)
{
	int fd;

	if((fd = create(file, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", file);
	write(fd, s, strlen(s));
	close(fd);
}

/* a JSON string of bytes: " \ and control bytes escaped, the rest as they are */
static char*
esc(char *s)
{
	Buf b;
	char u[8];

	memset(&b, 0, sizeof b);
	bput(&b, "\"");
	for(; *s; s++){
		if(*s == '"') bput(&b, "\\\"");
		else if(*s == '\\') bput(&b, "\\\\");
		else if(*s == '\n') bput(&b, "\\n");
		else if(*s == '\r') bput(&b, "\\r");
		else if(*s == '\t') bput(&b, "\\t");
		else if((uchar)*s < 0x20){
			snprint(u, sizeof u, "\\u%04x", (uchar)*s);
			bput(&b, u);
		}else{
			u[0] = *s; u[1] = 0;
			bput(&b, u);
		}
	}
	bput(&b, "\"");
	return b.s;
}

/* the bytes of a string literal esc wrote */
static char*
unesc(char *s)
{
	char *o, *q;

	o = q = malloc(strlen(s) + 1);
	for(s++; *s && *s != '"'; s++){
		if(*s != '\\'){
			*q++ = *s;
			continue;
		}
		switch(*++s){
		case 'n': *q++ = '\n'; break;
		case 'r': *q++ = '\r'; break;
		case 't': *q++ = '\t'; break;
		case 'u': *q++ = strtol(smprint("%.4s", s+1), nil, 16); s += 4; break;
		default: *q++ = *s;
		}
	}
	*q = 0;
	return o;
}

static void
mkdirp(char *d)
{
	char *s, *q;

	s = strdup(d);
	for(q = s; ; q++){
		if(*q == '/' || *q == 0){
			if(q > s){
				int c = *q;
				*q = 0;
				if(access(s, AEXIST) < 0)
					close(create(s, OREAD, DMDIR|0777));
				*q = c;
			}
			if(*q == 0)
				break;
		}
	}
	free(s);
}

static char *part[] = { "snapshot", "trace", "checkpoint" };
static char *file[] = { "zone.snapshot.json", "zone.trace.jsonl", "zone.checkpoint.json" };

static void
bundle(int argc, char **argv)
{
	char *dir, *b[3];
	J *m;
	int i;

	if(argc < 5)
		fail("usage: bundle.py <zone> <snapshot.json> <trace.jsonl> <checkpoint.json> <out_dir>");
	for(i = 0; i < 3; i++)
		if(access(argv[1+i], AEXIST) < 0)
			fail(smprint("missing input: %s", argv[1+i]));
	dir = smprint("%s/%s", argv[4], argv[0]);
	mkdirp(dir);
	m = mk(Obj);
	put(m, "zone", str(argv[0]));
	for(i = 0; i < 3; i++)
		b[i] = slurp(argv[1+i]);
	for(i = 0; i < 3; i++){
		spit(smprint("%s/%s", dir, file[i]), b[i]);
		put(m, part[i], str(file[i]));
		put(m, smprint("%s_literal", part[i]), mk(Str));
		m->v[m->n-1]->s = esc(b[i]);
	}
	spit(smprint("%s/manifest.json", dir), canonical(m));
	print("%s/manifest.json\n", dir);
	exits(nil);
}

static void
restore(int argc, char **argv, int check)
{
	char *mp, *path[3];
	J *m, *f, *l;
	int i;

	if(argc < 1)
		fail("usage: restore.py <bundle_dir>");
	mp = smprint("%s/manifest.json", argv[0]);
	if(access(mp, AEXIST) < 0)
		fail("manifest.json not found");
	m = load(mp);
	for(i = 0; i < 3; i++){
		f = get(m, part[i]);
		path[i] = smprint("%s/%s", argv[0], f != nil && f->t == Str ? bare(f) : "");
	}
	for(i = 0; i < 3; i++)
		if(access(path[i], AEXIST) < 0)
			fail(smprint("missing bundle file: %s", path[i]));
	for(i = 0; check && i < 3; i++){
		l = get(m, smprint("%s_literal", part[i]));
		if(l == nil || l->t != Str || strcmp(unesc(l->s), slurp(path[i])) != 0){
			fprint(2, "HALT: %s literal mismatch\n", part[i]);
			exits("3");
		}
	}
	print("OK\n");
	exits(nil);
}

static char*
field(J *j)
{
	return j == nil ? "null" : canonical(j);
}

void
main(int argc, char *argv[])
{
	J *m;
	int check;

	check = 1;
	ARGBEGIN{
	case 'n':
		check = 0;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zshard [-n] bundle|restore|fields args");
	if(strcmp(argv[0], "bundle") == 0) bundle(argc-1, argv+1);
	if(strcmp(argv[0], "restore") == 0) restore(argc-1, argv+1, check);
	if(strcmp(argv[0], "fields") == 0 && argc == 2){
		m = load(argv[1]);
		print("%s|%s|%s|%s\n", field(get(m, "zone")), field(get(m, "snapshot")), field(get(m, "trace")), field(get(m, "checkpoint")));
		exits(nil);
	}
	sysfatal("unknown subcommand %s", argv[0]);
}
