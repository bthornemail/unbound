/*
 * ztime - metaverse-build's time engine (~/devops/metaverse-build runtime/time) in
 * Plan 9 C, literal, as zcheck: a checkpoint holds its snapshot literal, no sha256.
 *	ztime branch timeline parent out	branch.py: a new timeline from a parent checkpoint
 *	ztime materialize checkpoint trace start end out	materialize.py: a window replayed
 *	ztime merge timelineA timelineB checkpointA checkpointB	merge-check.py: MERGE_OK, or
 *						HALT: MergeDenied: why (3)
 *	ztime -n merge ...			the falsifier: a checkpoint's literal not checked
 *						against its snapshot
 * branch.py names the new timeline by a random uuid; here it is named by its address,
 * TIMELINE/PARENT, where it branches from: two branches from one point are one literal.
 * materialize starts from the checkpoint's literal, so its snapshot must be that literal's
 * bytes, else HALT: corruption (3), as zcheck's restore; the window is zcheck's (-c its
 * path), run as its own program, as materialize.py runs window-replay.py; it prints the
 * snapshot where the tool prints its sha256. merge compares literals where the tool
 * compares sha256s: each checkpoint's literal against its snapshot's bytes, then the two.
 * A malformed input writes its reason and exits "2". Strings are compared and written as
 * their raw literals and numbers as their digits, as in zworld.
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

static char *zcheck = "zcheck";
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

static void
deny(char *why)
{
	fprint(2, "HALT: MergeDenied: %s\n", why);
	exits("3");
}

/* Python's truth of a value */
static int
nonempty(J *j)
{
	if(j == nil || j->t == Null || j->t == False)
		return 0;
	if(j->t == Str)
		return strlen(j->s) > 2;
	if(j->t == Num)
		return strcmp(j->s, "0") != 0;
	return j->t == True || j->n > 0;
}

static int
same(J *a, J *b)
{
	return strcmp(a == nil ? "null" : canonical(a), b == nil ? "null" : canonical(b)) == 0;
}

/* the checkpoint's snapshot is its literal's bytes */
static int
whole(J *ck)
{
	J *s, *l;

	s = get(ck, "snapshot");
	l = get(ck, "snapshot_literal");
	return l != nil && access(bare(s), AREAD) == 0 && strcmp(slurp(bare(s)), canonical(l)) == 0;
}

static void
branch(int argc, char **argv)
{
	J *tl, *nodes, *n, *w;
	char *id;

	if(argc < 3)
		fail("usage: branch.py <timeline.json> <parent_checkpoint_id> <out_timeline.json>");
	tl = load(argv[0]);
	nodes = get(tl, "nodes");
	if(get(nodes, argv[1]) == nil)
		fail("parent checkpoint not found");
	w = get(tl, "timeline");
	id = smprint("%s/%s", w != nil && w->t == Str ? bare(w) : "", argv[1]);
	n = mk(Obj);
	w = get(tl, "world");
	put(n, "world", w != nil ? w : mk(Null));
	put(n, "timeline", str(id));
	put(n, "nodes", nodes);
	put(n, "root", str(argv[1]));
	spit(argv[2], canonical(n));
	print("%s\n", id);
	exits(nil);
}

static void
materialize(int argc, char **argv)
{
	J *ck, *s;
	int pid;
	Waitmsg *w;
	char *q;

	if(argc < 5)
		fail("usage: materialize.py <checkpoint.json> <trace.jsonl> <start_index> <end_index> <out_snapshot>");
	ck = load(argv[0]);
	s = get(ck, "snapshot");
	if(!nonempty(s))
		fail("checkpoint missing snapshot");
	if(!whole(ck)){
		fprint(2, "HALT: corruption\n");
		exits("3");
	}
	switch(pid = rfork(RFPROC|RFFDG)){
	case -1:
		sysfatal("fork: %r");
	case 0:
		execl(zcheck, "zcheck", "-w", zworld, "window", bare(s), argv[1], argv[2], argv[3], argv[4], nil);
		sysfatal("exec %s: %r", zcheck);
	}
	while((w = wait()) != nil && w->pid != pid)
		free(w);
	if(w != nil && w->msg[0] != 0){
		q = strrchr(w->msg, ':');
		q = q != nil ? q+1 : w->msg;
		while(*q == ' ')
			q++;
		exits(q);
	}
	print("\n");
	exits(nil);
}

static void
merge(int argc, char **argv, int check)
{
	J *a, *b, *ca, *cb;

	if(argc < 4)
		fail("usage: merge-check.py <timeline_a.json> <timeline_b.json> <checkpoint_a.json> <checkpoint_b.json>");
	a = load(argv[0]);
	b = load(argv[1]);
	if(!same(get(a, "world"), get(b, "world")))
		deny("world mismatch");
	ca = load(argv[2]);
	cb = load(argv[3]);
	if(!nonempty(get(ca, "snapshot")) || !nonempty(get(cb, "snapshot")))
		fail("checkpoint missing snapshot path");
	if(check && !whole(ca))
		deny("checkpoint A literal mismatch");
	if(check && !whole(cb))
		deny("checkpoint B literal mismatch");
	if(!same(get(ca, "snapshot_literal"), get(cb, "snapshot_literal")))
		deny("snapshot literal mismatch");
	print("MERGE_OK\n");
	exits(nil);
}

void
main(int argc, char *argv[])
{
	int check;

	check = 1;
	ARGBEGIN{
	case 'c':
		zcheck = EARGF(sysfatal("-c zcheck"));
		break;
	case 'w':
		zworld = EARGF(sysfatal("-w zworld"));
		break;
	case 'n':
		check = 0;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: ztime [-n] [-c zcheck] [-w zworld] branch|materialize|merge args");
	if(strcmp(argv[0], "branch") == 0) branch(argc-1, argv+1);
	if(strcmp(argv[0], "materialize") == 0) materialize(argc-1, argv+1);
	if(strcmp(argv[0], "merge") == 0) merge(argc-1, argv+1, check);
	sysfatal("unknown subcommand %s", argv[0]);
}
