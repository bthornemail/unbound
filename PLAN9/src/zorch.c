/*
 * zorch - omi-dom's shared/contrasting-orchestrator.js in Plan 9 C, with the
 * spatial-gnn and edge-ngram pieces it runs on: each epoch XORs into a node's
 * feature the features of its neighbours the filter admits; the digest is the
 * XOR of every node's; diff = digest ^ target; the next filter also requires
 * a neighbour's index to have diff's low bit; converge repeats to diff 0 or 16
 * steps. A filter here is the set of node indices it admits, so the module's
 * chained closures are masks ANDed.
 *	zorch converge graphs	every target for each graph (features 1<<v,
 *				epochs 2, the filter admitting all): name target
 *				steps fixed digest history
 *	zorch law graphs	for every filter and four epochs: one epoch moves
 *				the digest by the XOR of the admitted neighbours of
 *				odd degree, and only by that
 *	zorch test		the module selfTest, on its 4-cycle
 * The finding: a neighbour's feature enters the digest once for each of its
 * edges, so a node of even degree cancels. On a graph whose degrees are all
 * even (every cycle, the module's own test graph among them) no filter and no
 * epoch moves the digest: it is the XOR of the features, and converge reaches
 * diff 0 only for the target already equal to it.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { NV = 32, NE = 32 };

typedef struct Graph Graph;
struct Graph {
	char	*name;
	int	n;
	int	adj[NV][NE];	/* neighbour lists, repeats kept, as buildAdjacency */
	int	deg[NV];
};

static ulong
epochs(Graph *g, ulong *f, ulong filter, int k)
{
	ulong next[NV], d;
	int e, v, j;

	for(e = 0; e < k; e++){
		for(v = 0; v < g->n; v++){
			next[v] = f[v];
			for(j = 0; j < g->deg[v]; j++)
				if(filter >> g->adj[v][j] & 1)
					next[v] ^= f[g->adj[v][j]];
		}
		memmove(f, next, g->n * sizeof f[0]);
	}
	for(d = 0, v = 0; v < g->n; v++)
		d ^= f[v];
	return d;
}

/* the module's runGNN from the initial features 1<<v */
static ulong
digest(Graph *g, ulong filter, int k)
{
	ulong f[NV];
	int v;

	for(v = 0; v < g->n; v++)
		f[v] = 1UL << v;
	return epochs(g, f, filter, k);
}

static ulong
parity(ulong diff)
{
	ulong m;
	int v;

	for(m = 0, v = 0; v < NV; v++)
		if(((v ^ diff) & 1) == 0)
			m |= 1UL << v;
	return m;
}

static int
readgraphs(char *file, Graph *gs, int max)
{
	Biobuf *b;
	char *l, *f[NV+2], *e[NE];
	int n, i, k, j;

	if((b = Bopen(file, OREAD)) == nil)
		sysfatal("open %s: %r", file);
	for(n = 0; n < max && (l = Brdstr(b, '\n', 1)) != nil; n++){
		k = tokenize(l, f, nelem(f));
		gs[n].name = f[0];
		gs[n].n = atoi(f[1]);
		for(i = 0; i < gs[n].n; i++){
			gs[n].deg[i] = 0;
			if(strcmp(f[2+i], "-") == 0)
				continue;
			k = getfields(f[2+i], e, nelem(e), 0, ",");
			for(j = 0; j < k; j++)
				gs[n].adj[i][gs[n].deg[i]++] = atoi(e[j]);
		}
	}
	Bterm(b);
	return n;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

void
main(int argc, char *argv[])
{
	Graph gs[16], *g;
	int ng, i, t, s, k, v, even, moved, bad;
	ulong d, filter, f[NV], before, admitted;
	char hist[256], *h;

	if(argc < 2)
		sysfatal("usage: zorch converge graphs | law graphs | test");
	if(strcmp(argv[1], "test") == 0){
		Graph c4 = { "cycle4", 4, { {1, 3}, {0, 2}, {1, 3}, {2, 0} }, { 2, 2, 2, 2 } };

		d = digest(&c4, ~0UL, 2);
		check("has diff (a type check; cannot fail)", 1);
		check("has digest (a type check; cannot fail)", 1);
		check("aligned fixed: the target is the digest just computed", (digest(&c4, ~0UL, 2) ^ d) == 0);
		check("converge fixed: the same target again", (digest(&c4, ~0UL, 2) ^ d) == 0);
		exits(fails ? "fail" : nil);
	}
	if(argc != 3)
		sysfatal("usage: zorch converge graphs | law graphs | test");
	ng = readgraphs(argv[2], gs, nelem(gs));
	if(strcmp(argv[1], "converge") == 0){
		for(i = 0; i < ng; i++)
			for(g = &gs[i], t = 0; t < 1 << g->n; t++){
				filter = ~0UL;
				h = hist;
				*h = 0;
				for(s = 0; s < 16; s++){
					d = digest(g, filter, 2);
					h = seprint(h, hist + sizeof hist, "%s%lud", s ? "," : "", d ^ t);
					if((d ^ t) == 0)
						break;
					filter &= parity(d ^ t);
				}
				print("%s %d %d %d %lud %s\n", g->name, t, s < 16 ? s+1 : 16, (d ^ t) == 0, d, hist);
			}
		exits(nil);
	}
	if(strcmp(argv[1], "law") == 0){
		for(i = 0; i < ng; i++){
			g = &gs[i];
			for(even = 1, v = 0; v < g->n; v++)
				even &= (g->deg[v] & 1) == 0;
			bad = moved = 0;
			for(filter = 0; filter < 1UL << g->n; filter++){
				for(v = 0; v < g->n; v++)
					f[v] = 1UL << v;
				for(k = 0; k < 4; k++){
					for(before = 0, admitted = 0, v = 0; v < g->n; v++){
						before ^= f[v];
						if((filter >> v & 1) && (g->deg[v] & 1))
							admitted ^= f[v];
					}
					d = epochs(g, f, filter, 1);
					bad += d != (before ^ admitted);
					moved += d != before;
				}
			}
			check(smprint("%s: one epoch moves the digest by the admitted odd-degree features, %lud filters x 4 epochs", g->name, 1UL << g->n), bad == 0);
			if(even)
				check(smprint("%s: every degree even, so no filter and no epoch moves the digest", g->name), moved == 0);
			else
				check(smprint("%s: an odd degree, so the digest moves (%d of %lud epochs)", g->name, moved, 4UL << g->n), moved > 0);
		}
		exits(fails ? "fail" : nil);
	}
	sysfatal("usage: zorch converge graphs | law graphs | test");
}
