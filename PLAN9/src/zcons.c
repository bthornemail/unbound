/*
 * zcons - h2gnn's geometric consensus (~/devops/h2gnn-enhanced
 * src/core/geometric-subsidiarity.ts) in Plan 9 C, each decision with its
 * proof. A group of n takes the first of tetrahedron, cube, icosahedron and
 * dodecahedron with at least n vertices, the dodecahedron past 20 (the
 * engine's mapToGeometric; the octahedron is never taken by count). Its
 * threshold is p/V, the engine's comment's rule, derived from the Schlafli
 * symbol {p,q} alone: E = 2pq / (4 - (p-2)(q-2)), V = 2E/q. Consensus holds
 * when yes/n >= p/V, that is yes V >= p n: integers, no ratio taken.
 *	zcons		every n from 1 to 24, every count of yes: n yes solid yes|no
 *	zcons -p n yes	that decision's proof, one step a line
 *	zcons -d	the same, by the thresholds the engine's header states
 *			(3/4, 5/8, 4/6, 11/20, 9/20): the falsifier
 */
#include <u.h>
#include <libc.h>

typedef struct Solid Solid;
struct Solid {
	char	*name;
	int	p, q;		/* the Schlafli symbol {p,q} */
	int	dn, dd;		/* the header's stated threshold, dn/dd */
};

/* in the order the engine takes them by count; the octahedron, {3,4}, is not among them */
static Solid solids[] = {
	{ "tetrahedron", 3, 3, 3, 4 },
	{ "cube", 4, 3, 5, 8 },
	{ "icosahedron", 3, 5, 11, 20 },
	{ "dodecahedron", 5, 3, 9, 20 },
};

static int
edges(Solid *s)
{
	return 2*s->p*s->q / (4 - (s->p-2)*(s->q-2));
}

static int
vertices(Solid *s)
{
	return 2*edges(s) / s->q;
}

static Solid*
solidfor(int n)
{
	int i;

	for(i = 0; i < nelem(solids); i++)
		if(vertices(&solids[i]) >= n)
			return &solids[i];
	return &solids[nelem(solids)-1];
}

static int
consensus(Solid *s, int n, int yes, int stated)
{
	if(n == 0)
		return 0;
	if(stated)
		return yes*s->dd >= s->dn*n;
	return yes*vertices(s) >= s->p*n;
}

static void
proof(int n, int yes)
{
	Solid *s;
	int e, v, d;

	s = solidfor(n);
	e = edges(s);
	v = vertices(s);
	d = 4 - (s->p-2)*(s->q-2);
	print("decision n %d yes %d\n", n, yes);
	print("  solid %s {%d,%d}: the first of tetrahedron cube icosahedron dodecahedron with V >= %d%s\n",
		s->name, s->p, s->q, n, v >= n ? "" : ", none, so the last");
	print("  E = 2pq / (4 - (p-2)(q-2)) = %d / %d = %d\n", 2*s->p*s->q, d, e);
	print("  V = 2E / q = %d / %d = %d\n", 2*e, s->q, v);
	print("  threshold p/V = %d/%d\n", s->p, v);
	print("  yes V = %d %s p n = %d: %s\n", yes*v, yes*v >= s->p*n ? ">=" : "<", s->p*n,
		consensus(s, n, yes, 0) ? "consensus" : "no consensus");
}

void
main(int argc, char *argv[])
{
	int n, yes, stated, prove;

	stated = prove = 0;
	ARGBEGIN{
	case 'p':
		prove = 1;
		break;
	case 'd':
		stated = 1;
		break;
	default:
		sysfatal("usage: zcons [-d] | -p n yes");
	}ARGEND
	if(prove){
		if(argc != 2)
			sysfatal("usage: zcons -p n yes");
		proof(atoi(argv[0]), atoi(argv[1]));
		exits(nil);
	}
	for(n = 1; n <= 24; n++)
		for(yes = 0; yes <= n; yes++)
			print("%d %d %s %s\n", n, yes, solidfor(n)->name, consensus(solidfor(n), n, yes, stated) ? "yes" : "no");
	exits(nil);
}
