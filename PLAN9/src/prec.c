/*
 * prec - prec.awk in Plan 9 C, line for line: the Wirth-Weber precedence
 * relations of every wsn.awk file read, taken together as one grammar.
 *
 *	prec files... >relations
 *
 * A LITERAL is a terminal, the same symbol in every file; a production name
 * is a nonterminal of its own file (file:NAME). For each production
 * A -> ... X Y ...:
 *	X ≐ Y,  X ⋖ Head+(Y),  Tail+(X) ⋗ Head*(Y)
 * and for each file's start symbol S:  $ ⋖ Head+(S),  Tail+(S) ⋗ $.
 * Each line out: X, the relation, Y, and the files it holds in (tab apart).
 * Only wsn.awk's file form is read: a first production NAME = NAMES . and
 * productions NAME = followed by one literal a line ("  ...").
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

typedef struct Prod Prod;
struct Prod {
	char	*name;		/* file:NAME */
	char	*file;
	char	**rhs;
	int	n;
	int	cap;
	Prod	*next;		/* in its hash chain */
};

typedef struct Rel Rel;
struct Rel {
	char	*x;
	char	*r;
	char	*y;
	char	*files;		/* space-separated, in the order first held */
	Rel	*next;
};

enum { NH = 1<<16 };
static Prod *ptab[NH];
static Prod **plist;
static int nprod, pcap;
static Rel *rtab[NH];
static Rel **rlist;
static int nrel, rcap;
static char **starts, **startfile;
static int nstart;

static uint
hash(char *s)
{
	uint h;

	for(h = 0; *s; s++)
		h = h*31 + (uchar)*s;
	return h % NH;
}

static Prod*
nt(char *x)
{
	Prod *p;

	for(p = ptab[hash(x)]; p != nil; p = p->next)
		if(strcmp(p->name, x) == 0)
			return p;
	return nil;
}

static Prod*
newprod(char *name, char *file)
{
	Prod *p;
	uint h;

	if((p = nt(name)) != nil){
		p->n = 0;	/* awk: len[cur] = 0 */
		return p;
	}
	p = mallocz(sizeof *p, 1);
	p->name = strdup(name);
	p->file = file;
	h = hash(name);
	p->next = ptab[h];
	ptab[h] = p;
	if(nprod == pcap){
		pcap = pcap ? 2*pcap : 1024;
		plist = realloc(plist, pcap*sizeof plist[0]);
	}
	plist[nprod++] = p;
	return p;
}

static void
push(Prod *p, char *s)
{
	if(p->n == p->cap){
		p->cap = p->cap ? 2*p->cap : 8;
		p->rhs = realloc(p->rhs, p->cap*sizeof p->rhs[0]);
	}
	p->rhs[p->n++] = strdup(s);
}

/* a set of symbols, as awk's SUBSEP-joined strings */
typedef struct Set Set;
struct Set {
	char	**s;
	int	n;
	int	cap;
};

static void
setadd(Set *s, char *x)
{
	if(x == nil || x[0] == 0)
		return;
	if(s->n == s->cap){
		s->cap = s->cap ? 2*s->cap : 8;
		s->s = realloc(s->s, s->cap*sizeof s->s[0]);
	}
	s->s[s->n++] = x;
}

/* awk head(x, plus): the first symbol and, while it is a nonterminal, its heads */
static void
head(char *x, int plus, Set *out)
{
	Prod *p;
	char *h;

	if((p = nt(x)) == nil){
		if(!plus)
			setadd(out, x);
		return;
	}
	if(p->n == 0)
		return;
	h = p->rhs[0];
	setadd(out, h);
	if(nt(h) != nil)
		head(h, 1, out);
}

/* awk tail(x) */
static void
tail(char *x, Set *out)
{
	Prod *p;
	char *t;

	if((p = nt(x)) == nil || p->n == 0)
		return;
	t = p->rhs[p->n-1];
	setadd(out, t);
	if(nt(t) != nil)
		tail(t, out);
}

static void
add(char *x, char *r, char *y, char *f)
{
	Rel *e;
	uint h;
	char *k, *fs;
	int n;

	if(x == nil || y == nil || x[0] == 0 || y[0] == 0)
		return;
	k = smprint("%s\t%s\t%s", x, r, y);
	h = hash(k);
	free(k);
	for(e = rtab[h]; e != nil; e = e->next)
		if(strcmp(e->x, x) == 0 && strcmp(e->r, r) == 0 && strcmp(e->y, y) == 0)
			break;
	if(e == nil){
		e = mallocz(sizeof *e, 1);
		e->x = x; e->r = r; e->y = y;
		e->files = strdup("");
		e->next = rtab[h];
		rtab[h] = e;
		if(nrel == rcap){
			rcap = rcap ? 2*rcap : 4096;
			rlist = realloc(rlist, rcap*sizeof rlist[0]);
		}
		rlist[nrel++] = e;
	}
	/* held[k, f]: each file once */
	n = strlen(f);
	for(fs = e->files; (fs = strstr(fs, f)) != nil; fs += n)
		if((fs == e->files || fs[-1] == ' ') && (fs[n] == 0 || fs[n] == ' '))
			return;
	fs = smprint("%s%s%s", e->files, e->files[0] ? " " : "", f);
	free(e->files);
	e->files = fs;
}

static void
addset(Set *xs, char *r, Set *ys, char *f)
{
	int i, j;

	for(i = 0; i < xs->n; i++)
		for(j = 0; j < ys->n; j++)
			add(xs->s[i], r, ys->s[j], f);
}

static void
read1(char *file)
{
	Biobuf *b;
	char *l, *w[512], *s, *fl;
	Prod *cur, *p;
	int n, i, started;

	if((b = Bopen(file, OREAD)) == nil)
		sysfatal("%s: %r", file);
	fl = strdup(file);
	cur = nil;
	started = 0;
	while((l = Brdstr(b, '\n', 1)) != nil){
		n = strlen(l);
		if(!started && n > 4 && l[0] >= 'A' && l[0] <= 'Z' && strstr(l, " = ") != nil && strcmp(l+n-2, " .") == 0){
			s = strdup(l);
			n = getfields(s, w, nelem(w), 1, " ");
			p = newprod(smprint("%s:%s", fl, w[0]), fl);
			for(i = 2; i < n && strcmp(w[i], ".") != 0; i++)
				push(p, smprint("%s:%s", fl, w[i]));
			starts = realloc(starts, (nstart+1)*sizeof starts[0]);
			startfile = realloc(startfile, (nstart+1)*sizeof startfile[0]);
			starts[nstart] = p->name;
			startfile[nstart++] = fl;
			started = 1;
		}else if(n > 2 && l[0] >= 'A' && l[0] <= 'Z' && strcmp(l+n-2, " =") == 0){
			l[n-2] = 0;
			cur = newprod(smprint("%s:%s", fl, l), fl);
		}else if(cur != nil && strncmp(l, "  \"", 3) == 0)
			push(cur, l+2);
		free(l);
	}
	Bterm(b);
}

void
main(int argc, char **argv)
{
	Biobuf out;
	Set hy, ty, hs, dollar;
	Prod *p;
	Rel *e;
	int i, j, k;

	for(i = 1; i < argc; i++)
		read1(argv[i]);
	for(k = 0; k < nprod; k++){
		p = plist[k];
		for(i = 0; i+1 < p->n; i++){
			add(p->rhs[i], "≐", p->rhs[i+1], p->file);
			memset(&hy, 0, sizeof hy);
			head(p->rhs[i+1], 1, &hy);
			memset(&ty, 0, sizeof ty);
			setadd(&ty, p->rhs[i]);
			addset(&ty, "⋖", &hy, p->file);
			memset(&ty, 0, sizeof ty);
			tail(p->rhs[i], &ty);
			memset(&hs, 0, sizeof hs);
			head(p->rhs[i+1], 0, &hs);
			head(p->rhs[i+1], 1, &hs);
			for(j = 0; j < hs.n; j++)	/* awk terms(): the terminals only */
				if(nt(hs.s[j]) != nil)
					hs.s[j] = "";
			addset(&ty, "⋗", &hs, p->file);
		}
	}
	memset(&dollar, 0, sizeof dollar);
	setadd(&dollar, "$");
	for(k = 0; k < nstart; k++){
		memset(&hy, 0, sizeof hy);
		head(starts[k], 1, &hy);
		addset(&dollar, "⋖", &hy, startfile[k]);
		memset(&ty, 0, sizeof ty);
		tail(starts[k], &ty);
		addset(&ty, "⋗", &dollar, startfile[k]);
	}
	Binit(&out, 1, OWRITE);
	for(k = 0; k < nrel; k++){
		e = rlist[k];
		Bprint(&out, "%s\t%s\t%s\t%s\n", e->x, e->r, e->y, e->files);
	}
	Bterm(&out);
	exits(nil);
}
