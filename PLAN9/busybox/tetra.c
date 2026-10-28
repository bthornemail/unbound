/*
 * tetra - XOR byte-distance is a metric: the tetrahedron's six edges
 * (B-TETRA.sh). The Hamming distance of two equal-length strings is the
 * popcount of their byte-wise XOR.
 */
#include <u.h>
#include <libc.h>

static char *V[] = { "WORD", "GODS", "LEFT", "rite" };

static int
ham(char *a, char *b)
{
	int t, z;

	for(t = 0; *a; a++, b++)
		for(z = (uchar)*a ^ (uchar)*b; z; z >>= 1)
			t += z & 1;
	return t;
}

static void
fail(char *s)
{
	fprint(2, "FAIL(TETRA): %s\n", s);
	exits("fail");
}

void
main(void)
{
	int a, b, c, n;

	n = nelem(V);
	/* assert: identity - a vertex against itself is distance 0 (a copy, the mirror) */
	for(a = 0; a < n; a++)
		if(ham(V[a], V[a]) != 0)
			fail(smprint("d(%s,%s) != 0", V[a], V[a]));
	/* assert: symmetry - d(A,B) = d(B,A) over all six unordered pairs */
	for(a = 0; a < n; a++)
		for(b = a + 1; b < n; b++)
			if(ham(V[a], V[b]) != ham(V[b], V[a]))
				fail(smprint("d(%s,%s) != d(%s,%s)", V[a], V[b], V[b], V[a]));
	/* assert: triangle inequality - d(A,C) <= d(A,B) + d(B,C) for all triples */
	for(a = 0; a < n; a++)
		for(b = 0; b < n; b++)
			for(c = 0; c < n; c++)
				if(ham(V[a], V[c]) > ham(V[a], V[b]) + ham(V[b], V[c]))
					fail(smprint("triangle fails: d(%s,%s) > d(%s,%s)+d(%s,%s)", V[a], V[c], V[a], V[b], V[b], V[c]));
	print("TETRA: XOR byte-distance is a metric (identity, symmetry, triangle) - passed\n");
	exits(nil);
}
