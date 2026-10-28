/*
 * zspan - every term as a path between YHWH and YAHWEH.
 *
 *	zspan <text
 *
 * The first line is the 0 end and the second the 1 end; a byte past the end
 * is 0. Where the ends differ, bit by bit in byte order and from the high
 * bit down, a term follows one end: 0 where it is the first's bit, 1 where
 * it is the second's. That string is the term's path. Where the ends agree
 * and the term does not, the bits print as braille cells (U+2800 + byte).
 * The path goes to fd 1, the off cells to fd 2: 0 catch, 1 default,
 * 2 throw (I-120, I-136). Nothing is counted. For every term, its two
 * chains, (term ^ first) and (term ^ second), XOR to first ^ second at
 * every byte: the term cancels, the shared zero of I-109.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static uchar
at(char *s, int n, int i)
{
	return i < n ? s[i] : 0;
}

void
main(void)
{
	Biobuf in, out, err;
	char *z, *o, *t;
	int nz, no, nt, n, i, j, k;
	uchar d, x;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	Binit(&err, 2, OWRITE);
	if((z = Brdstr(&in, '\n', 1)) == nil || (o = Brdstr(&in, '\n', 1)) == nil)
		sysfatal("need the two ends");
	nz = strlen(z);
	no = strlen(o);
	for(k = 0; (t = k == 0 ? strdup(z) : k == 1 ? strdup(o) : Brdstr(&in, '\n', 1)) != nil; k++){
		nt = strlen(t);
		n = nz > no ? nz : no;
		Bprint(&out, "%s\t", t);
		for(i = 0; i < (nt > n ? nt : n); i++)
			if(((at(t, nt, i) ^ at(z, nz, i)) ^ (at(t, nt, i) ^ at(o, no, i))) != (at(z, nz, i) ^ at(o, no, i)))
				sysfatal("%s: the chains do not share their zero at byte %d", t, i);
		for(i = 0; i < n; i++){
			d = at(z, nz, i) ^ at(o, no, i);
			x = at(t, nt, i);
			for(j = 7; j >= 0; j--)
				if(d >> j & 1)
					Bprint(&out, "%c", (x ^ at(z, nz, i)) >> j & 1 ? '1' : '0');
		}
		Bprint(&out, "\n");
		Bprint(&err, "%s\t", t);
		if(nt > n)
			n = nt;
		for(i = 0; i < n; i++){
			d = at(z, nz, i) ^ at(o, no, i);
			Bprint(&err, "%C", (Rune)(0x2800 + ((at(t, nt, i) ^ at(z, nz, i)) & ~d)));
		}
		Bprint(&err, "\n");
		free(t);
	}
	Bterm(&out);
	Bterm(&err);
	exits(nil);
}
