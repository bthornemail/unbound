/*
 * ztetra - four files as the vertices of a tetrahedron; each edge is the
 * parity (byte-wise XOR, and its complement XNOR) of the two files it joins.
 *
 *	ztetra <four lines
 *
 * Prints the six edges as braille cells (U+2800 + byte) and checks, failing
 * on the first that does not hold:
 *	each of the four faces closes: its three XOR edges XOR to all zero
 *	bytes, and its three XNOR edges to all 0xFF
 *	each file is recovered from any other file and the edge between them
 *	P = A ^ B ^ C ^ D recovers any one file from the other three
 * A byte past the end of a shorter file is 0.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { N = 4, Max = 1024 };

static uchar f[N][Max];
static char *name[N];
static int len;
static int E[6][2] = { {0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3} };
static int F[4][3] = { {0,1,2}, {0,1,3}, {0,2,3}, {1,2,3} };

static void
cells(Biobuf *b, uchar *x)
{
	int i;

	for(i = 0; i < len; i++)
		Bputrune(b, 0x2800 + x[i]);
}

static void
fail(char *s)
{
	fprint(2, "ztetra: FAIL %s\n", s);
	exits("fail");
}

static int
edge(int a, int b)
{
	int e;

	for(e = 0; e < 6; e++)
		if(E[e][0] == a && E[e][1] == b || E[e][0] == b && E[e][1] == a)
			return e;
	return -1;
}

void
main(void)
{
	Biobuf in, out;
	char *line;
	uchar xr[6][Max], xn[6][Max], p[Max], t[Max];
	int v, e, i, k, a, b;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	for(v = 0; v < N; v++){
		if((line = Brdstr(&in, '\n', 1)) == nil)
			sysfatal("need four lines");
		name[v] = line;
		if(strlen(line) > len)
			len = strlen(line);
		memmove(f[v], line, strlen(line));
	}
	for(e = 0; e < 6; e++)
		for(i = 0; i < len; i++){
			xr[e][i] = f[E[e][0]][i] ^ f[E[e][1]][i];
			xn[e][i] = xr[e][i] ^ 0xFF;
		}
	for(e = 0; e < 6; e++){
		Bprint(&out, "edge %s | %s\n  XOR  ", name[E[e][0]], name[E[e][1]]);
		cells(&out, xr[e]);
		Bprint(&out, "\n  XNOR ");
		cells(&out, xn[e]);
		Bprint(&out, "\n");
	}
	/* the faces close */
	for(k = 0; k < 4; k++){
		int ab = edge(F[k][0], F[k][1]), bc = edge(F[k][1], F[k][2]), ca = edge(F[k][2], F[k][0]);
		for(i = 0; i < len; i++){
			if((xr[ab][i] ^ xr[bc][i] ^ xr[ca][i]) != 0)
				fail(smprint("face %d: XOR edges do not close at byte %d", k, i));
			if((xn[ab][i] ^ xn[bc][i] ^ xn[ca][i]) != 0xFF)
				fail(smprint("face %d: XNOR edges do not close to 0xFF at byte %d", k, i));
		}
		Bprint(&out, "face %s | %s | %s: closes (XOR to 00, XNOR to FF)\n", name[F[k][0]], name[F[k][1]], name[F[k][2]]);
	}
	/* each file from any other file and their edge */
	for(a = 0; a < N; a++)
		for(b = 0; b < N; b++){
			if(a == b)
				continue;
			e = edge(a, b);
			for(i = 0; i < len; i++)
				if((f[a][i] ^ xr[e][i]) != f[b][i])
					fail(smprint("%s from %s and their edge", name[b], name[a]));
		}
	Bprint(&out, "every file recovered from any other file and the edge between them (12 ways)\n");
	/* one parity for all four */
	for(i = 0; i < len; i++)
		p[i] = f[0][i] ^ f[1][i] ^ f[2][i] ^ f[3][i];
	Bprint(&out, "P = A ^ B ^ C ^ D  ");
	cells(&out, p);
	Bprint(&out, "\n");
	for(v = 0; v < N; v++){
		for(i = 0; i < len; i++){
			t[i] = p[i];
			for(k = 0; k < N; k++)
				if(k != v)
					t[i] ^= f[k][i];
		}
		if(memcmp(t, f[v], len) != 0)
			fail(smprint("%s not recovered from P and the other three", name[v]));
	}
	Bprint(&out, "each file recovered from P and the other three (4 ways)\n");
	Bterm(&out);
	exits(nil);
}
