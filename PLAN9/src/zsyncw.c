/*
 * zsyncw - metaverse-build's sync transport and sync world (~/devops/metaverse-build
 * runtime/sync-transport, runtime/sync-world) in Plan 9 C:
 *	zsyncw receive log		receive.sh: each line of standard input appended to log but
 *					the empty ones; as `read -r` reads them, a last line without
 *					its newline is not read (the transport is lossy, CONTRACT.md)
 *	zsyncw merge logdir out		merge.sh: every envelope of the .jsonl logs in logdir in one
 *					order, peer by bytes then seq by number, written compact
 *					with its keys in their order; prints its first 200 lines
 *	zsyncw apply base merged out halts	apply-merged.sh: base copied to out, then each
 *					envelope's event folded by zworld (-w its path) as its own
 *					program; a HALT logged "HALT i why" and the state kept;
 *					any other refusal ends it, with the fold's exit; prints the
 *					snapshot where the tool prints its sha256
 *	zsyncw -l merge ...		the falsifier: logs taken in the order the directory gives
 * Two envelopes with one peer and seq keep the order of their logs' names, then of their
 * lines; merge.sh keeps os.listdir's, which the filesystem chooses. apply reads each
 * envelope as JSON; apply-merged.sh pastes it into Python source between ''', where a
 * JSON escape is read as Python's. Strings are compared and written as their raw literals
 * and numbers as their digits, as in zworld.
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

/* compact, the keys in their order: json.dumps(env, separators=(",", ":")) */
static void
compact(Buf *b, J *j)
{
	int i;

	switch(j->t){
	case Null: bput(b, "null"); return;
	case True: bput(b, "true"); return;
	case False: bput(b, "false"); return;
	case Num: case Str: bput(b, j->s); return;
	}
	bput(b, j->t == Arr ? "[" : "{");
	for(i = 0; i < j->n; i++){
		if(i) bput(b, ",");
		if(j->t == Obj){
			bput(b, j->k[i]);
			bput(b, ":");
		}
		compact(b, j->v[i]);
	}
	bput(b, j->t == Arr ? "]" : "}");
}

static char*
compacts(J *j)
{
	Buf b;

	memset(&b, 0, sizeof b);
	bput(&b, "");
	compact(&b, j);
	return b.s;
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
dirof(char *f)
{
	char *s, *q;

	s = strdup(f);
	if((q = strrchr(s, '/')) != nil){
		*q = 0;
		mkdirp(s);
	}
	free(s);
}

static void
append(char *file, char *s)
{
	int fd;

	if((fd = open(file, OWRITE)) < 0 && (fd = create(file, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", file);
	seek(fd, 0, 2);
	write(fd, s, strlen(s));
	close(fd);
}

static char*
readall(int fd)
{
	Buf b;
	char buf[8192];
	int n;

	memset(&b, 0, sizeof b);
	bput(&b, "");
	while((n = read(fd, buf, sizeof buf - 1)) > 0){
		buf[n] = 0;
		bput(&b, buf);
	}
	return b.s;
}

/* the lines `read -r` reads: each ended by its newline */
static int
rlines(char *s, char ***out)
{
	char **v, *nl;
	int n;

	v = nil;
	n = 0;
	for(; (nl = strchr(s, '\n')) != nil; s = nl+1){
		v = realloc(v, (n+1) * sizeof(char*));
		v[n++] = smprint("%.*s", (int)(nl - s), s);
	}
	*out = v;
	return n;
}

static void
receive(int argc, char **argv)
{
	char **v;
	int n, i;

	if(argc < 1)
		fail("usage: receive.sh <fifo|tcp> <source> <out_log>");
	dirof(argv[0]);
	n = rlines(readall(0), &v);
	for(i = 0; i < n; i++)
		if(v[i][0])
			append(argv[0], smprint("%s\n", v[i]));
	exits(nil);
}

typedef struct Env Env;
struct Env { char *peer; vlong seq; int ord; char *text; };

static int
envcmp(void *a, void *b)
{
	Env *x, *y;
	int c;

	x = a; y = b;
	if((c = strcmp(x->peer, y->peer)) != 0)
		return c;
	if(x->seq != y->seq)
		return x->seq < y->seq ? -1 : 1;
	return x->ord - y->ord;
}

static int
namecmp(void *a, void *b)
{
	return strcmp(((Dir*)a)->name, ((Dir*)b)->name);
}

static int
pyspace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void
merge(int argc, char **argv, int byname)
{
	Dir *d;
	Env *e;
	J *env, *pe, *sq;
	char *s, *l, *nl, *q, *end;
	int fd, nd, i, n, m, k;

	if(argc < 2)
		sysfatal("usage: zsyncw merge logdir out");
	e = nil;
	n = 0;
	nd = 0;
	if((fd = open(argv[0], OREAD)) >= 0){
		nd = dirreadall(fd, &d);
		close(fd);
	}
	if(byname)
		qsort(d, nd, sizeof(Dir), namecmp);
	for(i = 0; i < nd; i++){
		m = strlen(d[i].name);
		if(m < 6 || strcmp(d[i].name + m - 6, ".jsonl") != 0)
			continue;
		s = slurp(smprint("%s/%s", argv[0], d[i].name));
		for(l = s; *l; l = nl){
			nl = strchr(l, '\n');
			nl = nl ? nl+1 : l + strlen(l);
			for(q = l; q < nl && pyspace(*q); q++)
				;
			for(end = nl; end > q && pyspace(end[-1]); end--)
				;
			if(end == q)
				continue;
			p = smprint("%.*s", (int)(end - q), q);
			env = val();
			pe = get(env, "peer");
			sq = get(env, "seq");
			e = realloc(e, (n+1) * sizeof(Env));
			e[n].peer = pe == nil || pe->t == Null ? "None" : pe->t == Str ? bare(pe) : pe->s;
			e[n].seq = sq == nil ? 0 : strtoll(sq->t == Str ? bare(sq) : sq->s, nil, 10);
			e[n].ord = n;
			e[n].text = compacts(env);
			n++;
		}
	}
	qsort(e, n, sizeof(Env), envcmp);
	dirof(argv[1]);
	if((fd = create(argv[1], OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", argv[1]);
	for(i = 0, k = 0; i < n; i++){
		fprint(fd, "%s\n", e[i].text);
		if(k++ < 200)
			print("%s\n", e[i].text);
	}
	close(fd);
	exits(nil);
}

/* zworld base events > out, its standard error to err; its exit, nil when it wrote */
static char*
fold(char *base, char *events, char *out, char *err)
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
		if((fd = create(err, OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", err);
		dup(fd, 2);
		execl(zworld, "zworld", base, events, nil);
		sysfatal("exec %s: %r", zworld);
	}
	while((w = wait()) != nil && w->pid != pid)
		free(w);
	if(w == nil || w->msg[0] == 0)
		return nil;
	s = w->msg;
	if((q = strrchr(s, ':')) != nil)
		s = q + 1;
	while(*s == ' ')
		s++;
	return strdup(s);
}

static void
apply(int argc, char **argv)
{
	char **v, *tmp, *ev, *out, *err, *st, *msg;
	J *env, *e;
	int n, i, k;

	if(argc < 4)
		fail("usage: apply-merged.sh <base_snapshot> <merged_log> <out_snapshot> [halt_log]");
	spit(argv[2], slurp(argv[0]));
	spit(argv[3], "");
	n = rlines(slurp(argv[1]), &v);
	tmp = smprint("/tmp/zsyncw.%d", getpid());
	ev = smprint("%s.event", tmp);
	out = smprint("%s.out", tmp);
	err = smprint("%s.err", tmp);
	for(i = 0, k = 0; i < n; i++){
		if(v[i][0] == 0)
			continue;
		k++;
		p = v[i];
		env = val();
		e = get(env, "event");
		spit(ev, smprint("%s\n", e != nil ? compacts(e) : "{}"));
		st = fold(argv[2], ev, out, err);
		msg = slurp(err);
		if(st == nil)
			spit(argv[2], slurp(out));
		else if(strcmp(st, "3") == 0){
			while(*msg && msg[strlen(msg)-1] == '\n')
				msg[strlen(msg)-1] = 0;
			append(argv[3], smprint("HALT %d %s\n", k, msg));
		}else{
			fprint(2, "%s", msg);
			remove(ev); remove(out); remove(err);
			exits(st);
		}
	}
	remove(ev); remove(out); remove(err);
	print("%s\n", slurp(argv[2]));
	exits(nil);
}

void
main(int argc, char *argv[])
{
	int byname;

	byname = 1;
	ARGBEGIN{
	case 'w':
		zworld = EARGF(sysfatal("-w zworld"));
		break;
	case 'l':
		byname = 0;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zsyncw [-l] [-w zworld] receive log | merge logdir out | apply base merged out halts");
	if(strcmp(argv[0], "receive") == 0) receive(argc-1, argv+1);
	if(strcmp(argv[0], "merge") == 0) merge(argc-1, argv+1, byname);
	if(strcmp(argv[0], "apply") == 0) apply(argc-1, argv+1);
	sysfatal("unknown subcommand %s", argv[0]);
}
