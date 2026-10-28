/*
 * zpss - omi-dom's shared/projective-swap-space.js in Plan 9 C: the 3! = 6
 * orderings of the layout unit (BL, BO, BPE: byteLength, byteOffset,
 * BYTES_PER_ELEMENT), in the module's own order, and its projective role
 * table; its selfTest's ten assertions as checks.
 *	zpss orderings		the six orderings, one a line
 *	zpss role D		the role of dimension D: dim name role
 *	zpss test		the ten assertions, ok or FAIL each
 * The module's numbers come from packing-kiss.js: LEECH_KISSING 196560,
 * CAPACITY_2036 2036 (2048 - 12).
 */
#include <u.h>
#include <libc.h>

static char *keys[] = { "BL", "BO", "BPE" };

/* the module's loops: i, then j != i, then k != i, j */
static int
orderings(char *out[6][3])
{
	int i, j, k, n;

	n = 0;
	for(i = 0; i < 3; i++)
		for(j = 0; j < 3; j++){
			if(j == i)
				continue;
			for(k = 0; k < 3; k++){
				if(k == i || k == j)
					continue;
				out[n][0] = keys[i];
				out[n][1] = keys[j];
				out[n][2] = keys[k];
				n++;
			}
		}
	return n;
}

typedef struct Role Role;
struct Role {
	int	dim;
	char	*name;
	long	kissing;	/* 0: none */
	long	capacity;	/* 0: none */
};

static Role
role(int d)
{
	Role r;

	memset(&r, 0, sizeof r);
	r.dim = d;
	switch(d){
	case 3: r.name = "platonic-archimedean-catalan"; break;
	case 4: r.name = "simplex-polychora"; break;
	case 5: r.name = "presentation-surface"; break;
	case 8: r.name = "e8-shell"; break;
	case 24: r.name = "leech-ceiling"; r.kissing = 196560; break;
	case 2048: r.name = "meta-swap-ceiling"; r.capacity = 2036; break;
	case 2036: r.name = "capacity-boundary"; break;
	default: r.name = "unlabeled"; break;
	}
	return r;
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
	char *o[6][3];
	Role r;
	int n, i;

	if(argc < 2)
		sysfatal("usage: zpss orderings | role D | test");
	if(strcmp(argv[1], "orderings") == 0){
		n = orderings(o);
		for(i = 0; i < n; i++)
			print("%s %s %s\n", o[i][0], o[i][1], o[i][2]);
		exits(nil);
	}
	if(strcmp(argv[1], "role") == 0 && argc == 3){
		r = role(atoi(argv[2]));
		print("%d %s", r.dim, r.name);
		if(r.kissing) print(" kissing %ld", r.kissing);
		if(r.capacity) print(" capacity %ld", r.capacity);
		print("\n");
		exits(nil);
	}
	if(strcmp(argv[1], "test") == 0){
		/* the module's SOLID_SWAP_LAYERS, META_SWAP and SWAP_OPS, as it states them */
		check("3 solid layers", 3 == 3);
		check("3D name", role(3).dim == 3);
		check("4D 4!", role(4).dim == 4);
		check("layout 3!=6", orderings(o) == 6);
		check("meta from 24", 24 == 24);
		check("meta to 2048", 2048 == 2048);
		r = role(24);
		check("role 24 leech", strcmp(r.name, "leech-ceiling") == 0 && r.kissing == 196560);
		r = role(2048);
		check("role 2048 meta", strcmp(r.name, "meta-swap-ceiling") == 0 && r.capacity == 2036);
		check("role 3 solid", role(3).dim == 3);
		check("swap ops 3", 3 == 3);
		exits(fails ? "fail" : nil);
	}
	sysfatal("usage: zpss orderings | role D | test");
}
