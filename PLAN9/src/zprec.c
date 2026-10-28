/*
 * zprec - is a grammar in Wirth syntax notation a simple precedence grammar?
 *	zprec file.wsn
 * The four conditions (Wirth and Weber's, as Wikipedia's "Simple precedence
 * grammar" states them), one line each, then pass or fail:
 *	1 no erasing rules: in BNF, [ x ] and { x } derive the empty string,
 *	  so a production using them is erasing
 *	2 no useless rules: every production reachable from the first, and every
 *	  one productive (an identifier with no production derives nothing)
 *	3 one Wirth-Weber relation per pair of symbols: for A -> ... X Y ...,
 *	  X ≐ Y, X ⋖ Head+(Y), Tail+(X) ⋗ Head*(Y); $ ⋖ Head+(S), Tail+(S) ⋗ $
 *	4 uniquely invertible: no two productions with the same right side
 * ( x | y ) groups are distributed into their terms first. 3 and 4 are
 * decided only when 1 and 2 hold, since the relations are defined over an
 * erasing-free, useful grammar.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Fid, Flit, Fopt, Fgrp, Frep };
enum { MAXS = 512, MAXR = 4096 };

typedef struct Expr Expr;
typedef struct Fac Fac;
struct Fac {
	int	kind;
	char	*s;		/* Fid, Flit */
	Expr	*e;		/* Fopt, Fgrp, Frep */
};
struct Expr {
	Fac	**t[256];	/* terms */
	int	nf[256];
	int	n;
};

static char *src, *p;
static char *name[MAXS];
static Expr *body[MAXS];
static int nprod;

static void
skip(void)
{
	while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
}

static char*
ident(void)
{
	char *s;

	skip();
	s = p;
	while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')
		p++;
	if(s == p)
		return nil;
	return smprint("%.*s", (int)(p - s), s);
}

static char*
literal(void)
{
	Fmt f;

	skip();
	if(*p != '"')
		return nil;
	fmtstrinit(&f);
	fmtprint(&f, "\"");
	for(p++; *p; p++){
		if(*p == '"'){
			if(p[1] == '"'){
				fmtprint(&f, "\"\"");
				p++;
				continue;
			}
			p++;
			fmtprint(&f, "\"");
			return fmtstrflush(&f);
		}
		fmtprint(&f, "%c", *p);
	}
	sysfatal("a literal runs to the end");
	return nil;
}

static Expr *expr(void);

static Fac*
factor(void)
{
	Fac *f;
	char *s;
	int open;

	skip();
	f = mallocz(sizeof *f, 1);
	if((s = literal()) != nil){
		f->kind = Flit;
		f->s = s;
		return f;
	}
	if(*p == '[' || *p == '(' || *p == '{'){
		open = *p++;
		f->kind = open == '[' ? Fopt : open == '(' ? Fgrp : Frep;
		f->e = expr();
		skip();
		if(*p != (open == '[' ? ']' : open == '(' ? ')' : '}'))
			sysfatal("unclosed %c", open);
		p++;
		return f;
	}
	if((s = ident()) != nil){
		f->kind = Fid;
		f->s = s;
		return f;
	}
	free(f);
	return nil;
}

static Expr*
expr(void)
{
	Expr *e;
	Fac *f, *fs[256];
	int n;

	e = mallocz(sizeof *e, 1);
	for(;;){
		n = 0;
		while(n < nelem(fs) && (f = factor()) != nil)
			fs[n++] = f;
		e->t[e->n] = malloc(n*sizeof(Fac*) + 1);
		memmove(e->t[e->n], fs, n*sizeof(Fac*));
		e->nf[e->n++] = n;
		skip();
		if(*p != '|')
			return e;
		p++;
	}
}

static int
lookup(char *s)
{
	int i;

	for(i = 0; i < nprod; i++)
		if(strcmp(name[i], s) == 0)
			return i;
	return -1;
}

/* ---- the BNF rules: A -> X1 ... Xn, groups distributed ---- */
static int ra[MAXR];
static char **rx[MAXR];
static int rn[MAXR];
static int nr;
static int erasing[MAXS];

static void
emit(int a, char **seq, int n)
{
	if(nr == MAXR)
		sysfatal("more than %d rules", MAXR);
	ra[nr] = a;
	rx[nr] = malloc((n+1)*sizeof(char*));
	memmove(rx[nr], seq, n*sizeof(char*));
	rn[nr++] = n;
}

/* the sequences of a term from factor i on, appended to seq[0..n) */
static void
expand(int a, Fac **t, int nf, int i, char **seq, int n)
{
	Fac *f;
	int j;

	if(i == nf){
		emit(a, seq, n);
		return;
	}
	f = t[i];
	switch(f->kind){
	case Fid:
	case Flit:
		seq[n] = f->s;
		expand(a, t, nf, i+1, seq, n+1);
		break;
	case Fopt:
	case Frep:
		erasing[a] = 1;
		/* the erasing alternative, and the inner terms once (enough for 3 and 4 not to be decided) */
		expand(a, t, nf, i+1, seq, n);
		for(j = 0; j < f->e->n; j++){
			Fac *u[512];
			int k, m;
			m = 0;
			for(k = 0; k < f->e->nf[j]; k++) u[m++] = f->e->t[j][k];
			for(k = i+1; k < nf; k++) u[m++] = t[k];
			expand(a, u, m, 0, seq, n);
		}
		break;
	case Fgrp:
		for(j = 0; j < f->e->n; j++){
			Fac *u[512];
			int k, m;
			m = 0;
			for(k = 0; k < f->e->nf[j]; k++) u[m++] = f->e->t[j][k];
			for(k = i+1; k < nf; k++) u[m++] = t[k];
			expand(a, u, m, 0, seq, n);
		}
		break;
	}
}

static int
isnt(char *s)
{
	return s[0] != '"';
}

/* ---- symbol sets as small bitmaps over an interned symbol table ---- */
static char *sym[MAXS*4];
static int nsym;

static int
intern(char *s)
{
	int i;

	for(i = 0; i < nsym; i++)
		if(strcmp(sym[i], s) == 0)
			return i;
	if(nsym == nelem(sym))
		sysfatal("too many symbols");
	sym[nsym] = s;
	return nsym++;
}

typedef uchar Bits[MAXS*4/8];
#define SET(b, i)	((b)[(i)>>3] |= 1<<((i)&7))
#define HAS(b, i)	((b)[(i)>>3] & 1<<((i)&7))

static Bits headp[MAXS*4], tailp[MAXS*4];

void
main(int argc, char **argv)
{
	Biobuf *b;
	char *seq[1024], *n, *l, *why;
	int i, j, k, r, a, changed, ok1, ok2, ok3, ok4, reach[MAXS], prod[MAXS], x, y, conflicts, dups;
	Bits headstar;
	static char rel[MAXS*4][MAXS*4];	/* 0 none, else '=' '<' '>' */

	if(argc != 2){
		fprint(2, "usage: zprec file.wsn\n");
		exits("usage");
	}
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("%s: %r", argv[1]);
	src = Brdstr(b, 0, 0);
	if(src == nil){
		Fmt f;
		fmtstrinit(&f);
		Bseek(b, 0, 0);
		while((l = Brdstr(b, '\n', 0)) != nil){ fmtprint(&f, "%s", l); free(l); }
		src = fmtstrflush(&f);
	}
	Bterm(b);
	p = src;
	for(;;){
		skip();
		if(*p == 0)
			break;
		if((n = ident()) == nil)
			sysfatal("a production must start with an identifier at %.20s", p);
		skip();
		if(*p++ != '=')
			sysfatal("%s: no =", n);
		name[nprod] = n;
		body[nprod] = expr();
		skip();
		if(*p++ != '.')
			sysfatal("%s: no full stop", n);
		nprod++;
	}
	for(i = 0; i < nprod; i++)
		for(j = 0; j < body[i]->n; j++)
			expand(i, body[i]->t[j], body[i]->nf[j], 0, seq, 0);

	print("%s: %d productions, %d BNF rules\n", argv[1], nprod, nr);

	/* 1 erasing */
	ok1 = 1;
	for(i = 0; i < nprod; i++)
		if(erasing[i]){
			if(ok1) print("1 erasing rules:");
			print(" %s", name[i]);
			ok1 = 0;
		}
	for(r = 0; r < nr; r++)
		if(rn[r] == 0 && !erasing[ra[r]]){
			print("%s %s (empty)", ok1 ? "1 erasing rules:" : "", name[ra[r]]);
			ok1 = 0;
		}
	print(ok1 ? "1 no erasing rules: pass\n" : " — fail\n");

	/* 2 useless: productive (derives a terminal string), reachable from the first */
	memset(prod, 0, sizeof prod);
	do{
		changed = 0;
		for(r = 0; r < nr; r++){
			if(prod[ra[r]])
				continue;
			for(k = 0; k < rn[r]; k++)
				if(isnt(rx[r][k]) && ((a = lookup(rx[r][k])) < 0 || !prod[a]))
					break;
			if(k == rn[r]){
				prod[ra[r]] = 1;
				changed = 1;
			}
		}
	}while(changed);
	memset(reach, 0, sizeof reach);
	reach[0] = 1;
	do{
		changed = 0;
		for(r = 0; r < nr; r++){
			if(!reach[ra[r]])
				continue;
			for(k = 0; k < rn[r]; k++)
				if(isnt(rx[r][k]) && (a = lookup(rx[r][k])) >= 0 && !reach[a]){
					reach[a] = 1;
					changed = 1;
				}
		}
	}while(changed);
	ok2 = 1;
	for(i = 0; i < nprod; i++){
		why = !reach[i] ? "unreachable" : !prod[i] ? "unproductive" : nil;
		if(why != nil){
			print("%s %s (%s)", ok2 ? "2 useless rules:" : "", name[i], why);
			ok2 = 0;
		}
	}
	for(r = 0; r < nr; r++)
		for(k = 0; k < rn[r]; k++)
			if(isnt(rx[r][k]) && lookup(rx[r][k]) < 0){
				print("%s %s (no production)", ok2 ? "2 useless rules:" : "", rx[r][k]);
				ok2 = 0;
				rx[r][k] = smprint("%s", rx[r][k]);	/* reported once per use; harmless */
			}
	print(ok2 ? "2 no useless rules: pass\n" : " — fail\n");

	if(!ok1 || !ok2){
		print("3 relations: not decided (1 or 2 fails)\n4 invertibility: not decided (1 or 2 fails)\n");
		print("%s: not a simple precedence grammar\n", argv[1]);
		exits("fail");
	}

	/* Head+ and Tail+ over the BNF rules, to a fixed point */
	for(r = 0; r < nr; r++){
		a = intern(name[ra[r]]);
		SET(headp[a], intern(rx[r][0]));
		SET(tailp[a], intern(rx[r][rn[r]-1]));
	}
	intern("$");
	do{
		changed = 0;
		for(x = 0; x < nsym; x++)
			for(y = 0; y < nsym; y++){
				if(HAS(headp[x], y))
					for(k = 0; k < (int)sizeof(Bits); k++)
						if((headp[x][k] | headp[y][k]) != headp[x][k]){ headp[x][k] |= headp[y][k]; changed = 1; }
				if(HAS(tailp[x], y))
					for(k = 0; k < (int)sizeof(Bits); k++)
						if((tailp[x][k] | tailp[y][k]) != tailp[x][k]){ tailp[x][k] |= tailp[y][k]; changed = 1; }
			}
	}while(changed);

	/* 3 the relations, one per pair */
	conflicts = 0;
#define REL(X, R, Y) do{ int _x = (X), _y = (Y); \
	if(rel[_x][_y] && rel[_x][_y] != (R)){ if(!conflicts) print("3 conflicts:"); \
		if(conflicts < 12) print(" %s %c%c %s", sym[_x], rel[_x][_y], (R), sym[_y]); conflicts++; } \
	else rel[_x][_y] = (R); }while(0)
	for(r = 0; r < nr; r++)
		for(k = 0; k+1 < rn[r]; k++){
			x = intern(rx[r][k]);
			y = intern(rx[r][k+1]);
			REL(x, '=', y);
			for(j = 0; j < nsym; j++)
				if(HAS(headp[y], j))
					REL(x, '<', j);
			memset(headstar, 0, sizeof headstar);
			if(!isnt(sym[y]))
				SET(headstar, y);
			for(j = 0; j < nsym; j++)
				if(HAS(headp[y], j) && !isnt(sym[j]))
					SET(headstar, j);
			for(i = 0; i < nsym; i++)
				if(HAS(tailp[x], i))
					for(j = 0; j < nsym; j++)
						if(HAS(headstar, j))
							REL(i, '>', j);
		}
	a = intern(name[0]);
	for(j = 0; j < nsym; j++)
		if(HAS(headp[a], j))
			REL(intern("$"), '<', j);
	for(i = 0; i < nsym; i++)
		if(HAS(tailp[a], i))
			REL(i, '>', intern("$"));
	ok3 = conflicts == 0;
	k = 0;
	for(x = 0; x < nsym; x++)
		for(y = 0; y < nsym; y++)
			if(rel[x][y])
				k++;
	if(ok3)
		print("3 one relation per pair (%d pairs): pass\n", k);
	else
		print("%s — fail (%d)\n", conflicts > 12 ? " ..." : "", conflicts);

	/* 4 no two rules with one right side */
	dups = 0;
	for(r = 0; r < nr; r++)
		for(j = r+1; j < nr; j++){
			if(rn[r] != rn[j])
				continue;
			for(k = 0; k < rn[r]; k++)
				if(strcmp(rx[r][k], rx[j][k]) != 0)
					break;
			if(k == rn[r]){
				if(!dups) print("4 shared right sides:");
				if(dups < 8) print(" %s and %s", name[ra[r]], name[ra[j]]);
				dups++;
			}
		}
	ok4 = dups == 0;
	print(ok4 ? "4 uniquely invertible: pass\n" : " — fail\n");
	print("%s: %s\n", argv[1], ok3 && ok4 ? "a simple precedence grammar" : "not a simple precedence grammar");
	exits(ok3 && ok4 ? nil : "fail");
}
