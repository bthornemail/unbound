/*
 * zcodep - each file compared independently to a zeroth (YHWH, YAHWEH), and
 * the four files compared codependently to each other: the independent
 * signatures, XORed pairwise, give every codependent edge, because the
 * zeroth cancels: (A ^ Z) ^ (B ^ Z) = A ^ B.
 *
 *	zcodep <four files, then the zeroth, one per line
 *
 * Prints each file's signature against each zeroth (braille, U+2800 + byte)
 * and checks every pair against every zeroth, failing on the first that
 * does not hold. A byte past the end of a shorter line is 0.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { N = 4, Z = 8, Blk = 64 };

static uchar f[N][Blk], z[Z][Blk], sig[Z][N][Blk];
static char *fname[N], *zname[Z];
static int nz;

static void
cells(Biobuf *b, uchar *x)
{
	int i, n;

	for(n = Blk; n > 0 && x[n-1] == 0; n--)
		;
	for(i = 0; i < n; i++)
		Bputrune(b, 0x2800 + x[i]);
}

void
main(void)
{
	Biobuf in, out;
	char *line;
	int i, j, k, a, b, checks;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	for(i = 0; i < N; i++){
		if((line = Brdstr(&in, '\n', 1)) == nil)
			sysfatal("need four files");
		fname[i] = line;
		strncpy((char*)f[i], line, Blk);
	}
	while(nz < Z && (line = Brdstr(&in, '\n', 1)) != nil){
		zname[nz] = line;
		strncpy((char*)z[nz++], line, Blk);
	}
	/* independent: each file against each zeroth, on its own */
	for(k = 0; k < nz; k++)
		for(i = 0; i < N; i++){
			for(j = 0; j < Blk; j++)
				sig[k][i][j] = f[i][j] ^ z[k][j];
			Bprint(&out, "%-12s ^ %-7s ", fname[i], zname[k]);
			cells(&out, sig[k][i]);
			Bprint(&out, "\n");
		}
	/* codependent: every edge, from two independent signatures */
	checks = 0;
	for(k = 0; k < nz; k++)
		for(a = 0; a < N; a++)
			for(b = a + 1; b < N; b++){
				for(j = 0; j < Blk; j++)
					if((sig[k][a][j] ^ sig[k][b][j]) != (f[a][j] ^ f[b][j]))
						sysfatal("%s and %s against %s: the zeroth does not cancel at byte %d", fname[a], fname[b], zname[k], j);
				checks++;
			}
	Bprint(&out, "zcodep: %d checks: every edge between the files is the XOR of their two independent signatures, against each zeroth\n", checks);
	Bterm(&out);
	exits(nil);
}
