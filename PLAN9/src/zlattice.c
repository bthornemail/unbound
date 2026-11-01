/*
 * zlattice - metaverse-build's lattice (~/devops/metaverse-build runtime/lattice) in Plan 9 C,
 * literal: the observer, the graph and basis compiler, the plan projector and the tick that
 * rebinds on a changed plan, each as the tools' sed, awk, sort and read do it.
 *	zlattice observe root [beacons]	compiler/peer-observer.sh: each line read appended to
 *					observed.jsonl and traced as beacon_seen
 *	zlattice [-m max] [-a addr] tick root	reconcile/tick.sh: compile, project, and on a plan
 *					other than the last, trace rebind
 *	zlattice -n tick ...		the falsifier: the plan not compared, a rebind every tick
 * Literal, as zcheck: the tools' trace lines carry the clock, "t":`date +%s`, and these none;
 * tick keeps the last plan itself (plan/connection-plan.last) where the tool keeps its sha256;
 * the host's address is -a (127.0.0.1), where the projector asks the machine's routes; and
 * rebind.sh, which starts the posix bus's TCP listener, is traced and not run. As the tools do:
 * a line is read only with its newline (read -r); each field is sed's last match on its line;
 * seed files are joined as cat joins them, in byte order (the glob under LC_ALL=C); a peer's
 * fields are the last non-empty ones (awk); the nodes are sorted as sort -u sorts their lines
 * under LC_ALL=C; a node line is split as read splits on a tab IFS, empty fields collapsing
 * (bash's tab is IFS white space); the basis is sort -nr of score lines, ties by the whole
 * line reversed. With no seed file, or no observed.jsonl, the compiler's pipeline fails under
 * pipefail and tick exits "1" having written nothing. Outside the claim: an rtt with a leading
 * zero (bash reads it as octal), a peer holding a regular expression's characters (the
 * projector greps for it), values holding escaped quotes.
 */
#include <u.h>
#include <libc.h>
#include <ctype.h>

typedef struct Node Node;
struct Node { char *peer, *addr, *bus, *rtt; char *line; long score; };

static char *host = "127.0.0.1";
static int maxpeers = 1;
static int compare = 1;
static char *root;

static char*
path(char *rel)
{
	return smprint("%s/runtime/lattice/%s", root, rel);
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

static char*
readall(int fd)
{
	char *s, buf[8192];
	long n, len;

	s = strdup("");
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, len + n + 1);
		memmove(s + len, buf, n);
		len += n;
		s[len] = 0;
	}
	return s;
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
	char *s;

	s = strdup(f);
	*strrchr(s, '/') = 0;
	mkdirp(s);
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

static void
spit(char *file, char *s)
{
	int fd;

	if((fd = create(file, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", file);
	write(fd, s, strlen(s));
	close(fd);
}

/* the lines `while read -r` reads: each ended by its newline; the empty ones dropped */
static int
lines(char *s, char ***out)
{
	char **v, *nl;
	int n;

	v = nil;
	n = 0;
	for(; s != nil && (nl = strchr(s, '\n')) != nil; s = nl+1){
		if(nl == s)
			continue;
		v = realloc(v, (n+1) * sizeof(char*));
		v[n++] = smprint("%.*s", (int)(nl - s), s);
	}
	*out = v;
	return n;
}

/* sed -n 's/.*"KEY":"\([^"]*\)".*\/\1/p': the last "KEY":"..." on the line; nil when none */
static char*
strfield(char *l, char *key)
{
	char *pat, *p, *q, *last;

	pat = smprint("\"%s\":\"", key);
	last = nil;
	for(p = l; (p = strstr(p, pat)) != nil; p++)
		if(strchr(p + strlen(pat), '"') != nil)
			last = p + strlen(pat);
	free(pat);
	if(last == nil)
		return nil;
	q = strchr(last, '"');
	return smprint("%.*s", (int)(q - last), last);
}

/* sed -n 's/.*"KEY":\([0-9][0-9]*\).*\/\1/p': the last "KEY": followed by digits, all of them */
static char*
numfield(char *l, char *key)
{
	char *pat, *p, *q, *last;

	pat = smprint("\"%s\":", key);
	last = nil;
	for(p = l; (p = strstr(p, pat)) != nil; p++)
		if(isdigit(p[strlen(pat)]))
			last = p + strlen(pat);
	free(pat);
	if(last == nil)
		return nil;
	for(q = last; isdigit(*q); q++)
		;
	return smprint("%.*s", (int)(q - last), last);
}

static char *empty(char *s) { return s == nil ? "" : s; }

static void
observe(int argc, char **argv)
{
	char **v, *s;
	int n, i, fd;

	if(argc > 1 && strcmp(argv[1], "-") != 0){
		if((fd = open(argv[1], OREAD)) < 0)
			sysfatal("open %s: %r", argv[1]);
	}else
		fd = 0;
	s = readall(fd);
	dirof(path("peers/observe/observed.jsonl"));
	dirof(path("trace/discovery.log"));
	n = lines(s, &v);
	for(i = 0; i < n; i++){
		append(path("peers/observe/observed.jsonl"), smprint("%s\n", v[i]));
		append(path("trace/discovery.log"), smprint("{\"type\":\"beacon_seen\",\"raw\":%s}\n", v[i]));
	}
	exits(nil);
}

static int
namecmp(void *a, void *b)
{
	return strcmp(((Dir*)a)->name, ((Dir*)b)->name);
}

static int
linecmp(void *a, void *b)
{
	return strcmp(((Node*)a)->line, ((Node*)b)->line);
}

/* sort -nr: by the leading number, the larger first; a tie by the whole line, the larger first */
static int
scorecmp(void *a, void *b)
{
	Node *x, *y;

	x = a; y = b;
	if(x->score != y->score)
		return x->score > y->score ? -1 : 1;
	return strcmp(y->line, x->line);
}

/* IFS=$'\t' read -r a b c d: the fields between runs of tabs, the last taking the rest */
static void
split4(char *l, char **f)
{
	char *s, *e;
	int i;

	s = strdup(l);
	for(i = 0; i < 4; i++)
		f[i] = "";
	while(*s == '\t')
		s++;
	for(i = 0; i < 3 && *s; i++){
		e = strchr(s, '\t');
		if(e == nil){
			f[i] = s;
			return;
		}
		*e++ = 0;
		f[i] = s;
		while(*e == '\t')
			e++;
		s = e;
	}
	if(*s){
		for(e = s + strlen(s); e > s && e[-1] == '\t'; e--)
			;
		*e = 0;
		f[3] = s;
	}
}

static int
compile(void)
{
	Dir *d;
	Node *nd, *sc;
	char *seeds, *s, **v, *f[4], *sd;
	int fd, n, nn, ns, i, j, k, any;
	char *graph, *basis;

	dirof(path("graph/peergraph.json"));
	dirof(path("trace/routing.log"));
	sd = path("peers/seeds.d");
	seeds = strdup("");
	any = 0;
	if((fd = open(sd, OREAD)) >= 0){
		n = dirreadall(fd, &d);
		close(fd);
		qsort(d, n, sizeof(Dir), namecmp);
		for(i = 0; i < n; i++){
			k = strlen(d[i].name);
			if(d[i].name[0] == '.' || k < 5 || strcmp(d[i].name + k - 5, ".json") != 0)
				continue;
			s = slurp(smprint("%s/%s", sd, d[i].name));
			seeds = smprint("%s%s", seeds, empty(s));
			any = 1;
		}
	}
	if(!any || access(path("peers/observe/observed.jsonl"), AEXIST) < 0)
		return 0;	/* cat of an unmatched glob, or [ -f OBS ] false: the pipeline fails */
	nd = nil;
	nn = 0;
	for(k = 0; k < 2; k++){
		n = lines(k == 0 ? seeds : slurp(path("peers/observe/observed.jsonl")), &v);
		for(i = 0; i < n; i++){
			char *p, *a, *b, *r;

			p = strfield(v[i], "id");
			if(p == nil || *p == 0)
				p = strfield(v[i], "node");
			if(p == nil || *p == 0)
				continue;
			a = strfield(v[i], "addr");
			b = numfield(v[i], "bus");
			r = numfield(v[i], "rtt_ms");
			for(j = 0; j < nn && strcmp(nd[j].peer, p) != 0; j++)
				;
			if(j == nn){
				nd = realloc(nd, (nn+1) * sizeof(Node));
				memset(&nd[nn], 0, sizeof(Node));
				nd[nn++].peer = p;
			}
			if(a != nil && *a) nd[j].addr = a;
			if(b != nil && *b) nd[j].bus = b;
			if(r != nil && *r) nd[j].rtt = r;
		}
	}
	for(i = 0; i < nn; i++)
		nd[i].line = smprint("%s\t%s\t%s\t%s", nd[i].peer, empty(nd[i].addr), empty(nd[i].bus), empty(nd[i].rtt));
	qsort(nd, nn, sizeof(Node), linecmp);

	graph = strdup("{\n  \"version\": \"peergraph-1\",\n  \"nodes\": [\n");
	sc = malloc((nn+1) * sizeof(Node));
	ns = 0;
	for(i = 0; i < nn; i++){
		split4(nd[i].line, f);
		graph = smprint("%s%s    {\"id\":\"%s\",\"addr\":\"%s\",\"ports\":{\"bus\":%s},\"health\":{\"ok\":true,\"rtt_ms\":%s}}",
			graph, i ? ",\n" : "", f[0], *f[1] ? f[1] : "0.0.0.0", *f[2] ? f[2] : "0", *f[3] ? f[3] : "0");
		if(*f[2] == 0)
			continue;
		sc[ns].score = *f[3] ? 100000 - strtol(f[3], nil, 10) : 0;
		sc[ns].line = smprint("%ld\t%s\t%s\t%s", sc[ns].score, f[0], f[1], f[2]);
		ns++;
	}
	spit(path("graph/peergraph.json"), smprint("%s\n  ],\n  \"edges\": [],\n  \"simplices\": []\n}\n", graph));

	qsort(sc, ns, sizeof(Node), scorecmp);
	basis = strdup("{\n  \"version\": \"basis-1\",\n  \"selected\": [\n");
	for(i = 0, k = 0; i < ns && k < maxpeers; i++){
		split4(sc[i].line, f);
		basis = smprint("%s%s    {\"peer\":\"%s\",\"transport\":\"bus\",\"lane\":\"tcp\",\"score\":%ld.00}", basis, k ? ",\n" : "", f[1], sc[i].score);
		k++;
	}
	spit(path("graph/basis.json"), smprint("%s\n  ],\n  \"rejected\": []\n}\n", basis));
	append(path("trace/routing.log"), smprint("{\"type\":\"basis\",\"graph\":\"%s\",\"basis\":\"%s\"}\n", path("graph/peergraph.json"), path("graph/basis.json")));
	return 1;
}

/* lookup_peer: the first graph line holding "id":"PEER", its last addr and bus, else the defaults */
static void
lookup(char *graph, char *peer, char **addr, char **bus)
{
	char **v, *pat, *a, *b;
	int n, i;

	*addr = host;
	*bus = "7000";
	pat = smprint("\"id\":\"%s\"", peer);
	n = lines(graph, &v);
	for(i = 0; i < n; i++)
		if(strstr(v[i], pat) != nil){
			a = strfield(v[i], "addr");
			b = numfield(v[i], "bus");
			if(a != nil && *a) *addr = a;
			if(b != nil && *b) *bus = b;
			break;
		}
	free(pat);
}

static void
project(void)
{
	char *graph, *basis, **v, *p, *a, *b, *plan, *first;
	int n, i, k;

	dirof(path("plan/connection-plan.json"));
	graph = slurp(path("graph/peergraph.json"));
	basis = slurp(path("graph/basis.json"));
	plan = strdup("{\n  \"version\": \"plan-1\",\n  \"attachments\": [\n");
	first = nil;
	n = lines(basis, &v);
	for(i = 0, k = 0; i < n; i++){
		p = strfield(v[i], "peer");
		if(p == nil)
			continue;
		if(first == nil)
			first = p;
		if(*p == 0)
			continue;
		lookup(graph, p, &a, &b);
		plan = smprint("%s%s    {\"name\":\"bus-%s\",\"kind\":\"bus\",\"peer\":\"%s\",\"trace_fifo\":\"%s/pipelines/posix-bus/trace.fifo\",\"trace_tcp\":\"tcp://%s:%s\"}",
			plan, k ? ",\n" : "", p, p, root, a, b);
		k++;
	}
	spit(path("plan/connection-plan.json"), smprint("%s\n  ]\n}\n", plan));
	lookup(graph, first != nil ? first : "", &a, &b);
	if(first == nil || *first == 0){
		a = host;
		b = "7000";
	}
	spit(path("plan/device-plan.json"), smprint("{\"node_id\":\"host-001\",\"buses\":{\"trace\":{\"mode\":\"tcp\",\"addr\":\"%s\",\"port\":%s}}}\n", a, b));
}

static void
tick(void)
{
	char *plan, *last;

	if(!compile())
		exits("1");
	project();
	plan = slurp(path("plan/connection-plan.json"));
	last = slurp(path("plan/connection-plan.last"));
	if(!compare || last == nil || strcmp(plan, last) != 0){
		spit(path("plan/connection-plan.last"), plan);
		append(path("trace/routing.log"), smprint("{\"type\":\"rebind\",\"plan\":\"%s\"}\n", path("plan/connection-plan.json")));
	}
	exits(nil);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'm':
		maxpeers = atoi(EARGF(sysfatal("-m max")));
		break;
	case 'a':
		host = EARGF(sysfatal("-a addr"));
		break;
	case 'n':
		compare = 0;
		break;
	}ARGEND
	if(argc < 2)
		sysfatal("usage: zlattice [-n] [-m max] [-a addr] observe root [beacons] | tick root");
	root = argv[1];
	if(strcmp(argv[0], "observe") == 0) observe(argc-1, argv+1);
	if(strcmp(argv[0], "tick") == 0) tick();
	sysfatal("unknown subcommand %s", argv[0]);
}
