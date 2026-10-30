/*
 * zcheck - metaverse-build's checkpoints (~/devops/metaverse-build runtime/checkpoint)
 * in Plan 9 C, literal (the author's choice): where the tools keep a snapshot's
 * sha256 and a wall-clock timestamp, a checkpoint here keeps the snapshot literal
 * itself; restore compares the snapshot's bytes with it, and any difference is
 * HALT: corruption; prune orders checkpoints by their trace position, last_index,
 * not by the clock. The tools run the world fold as a separate program
 * (apply-event.py); so does this one (zworld, -w its path). Where the tools print
 * a sha256, this prints the snapshot literal.
 *	zcheck checkpoint zone base trace snapshot checkpoint
 *	zcheck restore checkpoint out
 *	zcheck rolling zone base trace dir id
 *	zcheck prune dir keep
 *	zcheck window base trace start end out
 *	zcheck fields checkpoint	zone|trace|snapshot|last_index|checkpoint_id
 *	zcheck verify checkpoint	"ok" when its literal is its snapshot's bytes
 * A malformed input writes its reason and exits "2"; the fold's own exit passes through.
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

static char *zworld = "zworld";
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

static J*
num(long n)
{
	J *j;

	j = mk(Num);
	j->s = smprint("%ld", n);
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

/* the trace's lines that are not blank, as the tools count and slice them */
static int
lines(char *file, char ***out)
{
	char *s, *l, *nl, *q, **v;
	int n, blank;

	s = slurp(file);
	v = nil;
	n = 0;
	for(l = s; *l; l = nl){
		nl = strchr(l, '\n');
		nl = nl ? nl+1 : l + strlen(l);
		blank = 1;
		for(q = l; q < nl; q++)
			if(*q != ' ' && *q != '\t' && *q != '\n' && *q != '\r' && *q != '\v' && *q != '\f')
				blank = 0;
		if(blank)
			continue;
		v = realloc(v, (n+1) * sizeof(char*));
		v[n++] = smprint("%.*s", (int)(nl - l), l);
	}
	*out = v;
	return n;
}

/* the fold, a program of its own: base and events in, the snapshot to out; its exit passed through */
static void
fold(char *base, char *events, char *out)
{
	int pid, fd;
	Waitmsg *w;
	char *s, *q;

	switch(pid = rfork(RFPROC|RFFDG)){
	case -1:
		sysfatal("fork: %r");
	case 0:
		if((fd = create(out, OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", out);
		dup(fd, 1);
		execl(zworld, "zworld", base, events, nil);
		sysfatal("exec %s: %r", zworld);
	}
	while((w = wait()) != nil && w->pid != pid)
		free(w);
	if(w == nil || w->msg[0] == 0)
		return;
	remove(out);	/* the fold wrote nothing it did not finish */
	s = w->msg;
	if((q = strrchr(s, ':')) != nil)
		s = q + 1;
	while(*s == ' ')
		s++;
	exits(s);
}

static J*
record(char *zone, char *trace, char *snap, int nlines, char *id)
{
	J *ck;

	ck = mk(Obj);
	put(ck, "zone", str(zone));
	put(ck, "trace", str(trace));
	put(ck, "snapshot", str(snap));
	put(ck, "last_index", num(nlines - 1));
	p = slurp(snap);
	put(ck, "snapshot_literal", val());
	if(id != nil)
		put(ck, "checkpoint_id", str(id));
	return ck;
}

static void
checkpoint(int argc, char **argv)
{
	char **v;
	J *st;

	if(argc < 5)
		fail("usage: checkpoint.py <zone> <base_snapshot.json> <trace.jsonl> <out_snapshot> <out_checkpoint>");
	st = get(load(argv[1]), "state");
	if(st == nil || st->t != Obj)
		fail("invalid snapshot: missing state");
	fold(argv[1], argv[2], argv[3]);
	if(argv[4][0])
		spit(argv[4], canonical(record(argv[0], argv[2], argv[3], lines(argv[2], &v), nil)));
	print("%s", slurp(argv[3]));
	exits(nil);
}

static void
rolling(int argc, char **argv)
{
	char *snap, *ck, **v;

	if(argc < 5)
		fail("usage: rolling-checkpoint.py <zone> <base_snapshot.json> <trace.jsonl> <out_dir> <checkpoint_id>");
	if(access(argv[3], AEXIST) < 0)
		close(create(argv[3], OREAD, DMDIR|0777));
	snap = smprint("%s/%s.snapshot.json", argv[3], argv[4]);
	ck = smprint("%s/%s.checkpoint.json", argv[3], argv[4]);
	fold(argv[1], argv[2], snap);
	spit(ck, canonical(record(argv[0], argv[2], snap, lines(argv[2], &v), argv[4])));
	print("%s\n", ck);
	exits(nil);
}

static void
restore(int argc, char **argv)
{
	J *ck, *trace, *base, *li, *literal;
	char **v, *tmp, *s, *b;
	int n, start, i, fd;

	if(argc < 2)
		fail("usage: restore.py <checkpoint.json> <out_snapshot>");
	ck = load(argv[0]);
	trace = get(ck, "trace");
	base = get(ck, "snapshot");
	li = get(ck, "last_index");
	literal = get(ck, "snapshot_literal");
	if(trace == nil || trace->t != Str || strlen(trace->s) == 2 || base == nil || base->t != Str || strlen(base->s) == 2 || literal == nil || literal->t == Null)
		fail("invalid checkpoint: missing fields");
	b = slurp(bare(base));
	if(strcmp(b, canonical(literal)) != 0){
		fprint(2, "HALT: corruption\n");
		exits("3");
	}
	n = lines(bare(trace), &v);
	start = li != nil && li->t == Num && strchr(li->s, '.') == nil ? atoi(li->s) + 1 : n;
	if(start >= n){
		spit(argv[1], b);
		print("%s", b);
		exits(nil);
	}
	tmp = smprint("/tmp/zcheck.%d", getpid());
	fd = create(tmp, OWRITE, 0666);
	for(i = start; i < n; i++)
		write(fd, v[i], strlen(v[i]));
	close(fd);
	fold(bare(base), tmp, argv[1]);
	remove(tmp);
	s = slurp(argv[1]);
	print("%s", s);
	exits(nil);
}

static void
window(int argc, char **argv)
{
	char **v, *tmp, *s;
	int n, a, e, i, fd;

	if(argc < 4)
		fail("usage: window-replay.py <base_snapshot.json> <trace.jsonl> <start_index> <end_index> <out_snapshot>");
	n = lines(argv[1], &v);
	a = atoi(argv[2]);
	e = atoi(argv[3]);
	if(a < 0 || e < a || e >= n)
		fail("invalid window indices");
	tmp = smprint("/tmp/zcheck.%d", getpid());
	fd = create(tmp, OWRITE, 0666);
	for(i = a; i <= e; i++)
		write(fd, v[i], strlen(v[i]));
	close(fd);
	fold(argv[0], tmp, argv[4]);
	remove(tmp);
	s = slurp(argv[4]);
	print("%s", s);
	exits(nil);
}

typedef struct Ck Ck;
struct Ck { char *name; long pos; };

static int
ckcmp(void *a, void *b)
{
	Ck *x, *y;

	x = a; y = b;
	if(x->pos != y->pos)
		return x->pos < y->pos ? -1 : 1;
	return strcmp(x->name, y->name);
}

static void
prune(int argc, char **argv)
{
	Dir *d;
	Ck *c;
	J *ck, *li, *snap;
	char *path;
	int fd, nd, n, i, k, cut, m;

	if(argc < 2)
		fail("usage: prune.py <checkpoint_dir> <keep_n>");
	k = atoi(argv[1]);
	if((fd = open(argv[0], OREAD)) < 0)
		fail("checkpoint_dir not found");
	nd = dirreadall(fd, &d);
	close(fd);
	c = malloc((nd+1) * sizeof(Ck));
	n = 0;
	for(i = 0; i < nd; i++){
		m = strlen(d[i].name);
		if(m < 16 || strcmp(d[i].name + m - 16, ".checkpoint.json") != 0)
			continue;
		c[n].name = strdup(d[i].name);
		ck = load(smprint("%s/%s", argv[0], c[n].name));
		li = get(ck, "last_index");
		c[n].pos = li != nil && li->t == Num ? atol(li->s) : -1;	/* unreadable first, as the tools put a missing timestamp */
		n++;
	}
	qsort(c, n, sizeof(Ck), ckcmp);
	cut = k > 0 ? (k < n ? n - k : 0) : (k == 0 ? 0 : (-k < n ? -k : n));	/* Python's [:-k] */
	for(i = 0; i < cut; i++){
		path = smprint("%s/%s", argv[0], c[i].name);
		snap = get(load(path), "snapshot");
		if(snap != nil && snap->t == Str && strlen(snap->s) > 2 && access(bare(snap), AEXIST) == 0)
			remove(bare(snap));
		remove(path);
	}
	print("removed %d\n", cut);
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
	J *ck, *s;

	ARGBEGIN{
	case 'w':
		zworld = EARGF(sysfatal("-w zworld"));
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zcheck [-w zworld] checkpoint|restore|rolling|prune|window|fields|verify args");
	if(strcmp(argv[0], "checkpoint") == 0) checkpoint(argc-1, argv+1);
	if(strcmp(argv[0], "restore") == 0) restore(argc-1, argv+1);
	if(strcmp(argv[0], "rolling") == 0) rolling(argc-1, argv+1);
	if(strcmp(argv[0], "prune") == 0) prune(argc-1, argv+1);
	if(strcmp(argv[0], "window") == 0) window(argc-1, argv+1);
	if(strcmp(argv[0], "fields") == 0){
		ck = load(argv[1]);
		print("%s|%s|%s|%s|%s\n", field(get(ck, "zone")), field(get(ck, "trace")), field(get(ck, "snapshot")),
			field(get(ck, "last_index")), field(get(ck, "checkpoint_id")));
		exits(nil);
	}
	if(strcmp(argv[0], "verify") == 0){
		ck = load(argv[1]);
		s = get(ck, "snapshot");
		if(s == nil || s->t != Str || strcmp(slurp(bare(s)), canonical(get(ck, "snapshot_literal"))) != 0){
			print("differs\n");
			exits("differs");
		}
		print("ok\n");
		exits(nil);
	}
	sysfatal("unknown subcommand %s", argv[0]);
}
