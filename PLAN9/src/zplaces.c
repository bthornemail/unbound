/*
 * zplaces - which Aegean places each code point of a text writes.
 *
 *	zplaces <text
 *
 * A code point's Aegean form writes a place when that decimal digit is not
 * 0: units, tens, hundreds, thousands, ten thousands. The written places are
 * a subset of the five, a vertex of the 5-cube; below 10,000 the subset is
 * of four places, a vertex of the 4-cube (proofs/svg/hasse/rules.svg).
 * Prints, for each subset used, its places (T H X U, thousands to units) and
 * how many code points write exactly it; fails if any writes ten thousands.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

void
main(void)
{
	Biobuf in;
	long c, used[32], first[32];
	int m, v, i, n;

	memset(used, 0, sizeof used);
	Binit(&in, 0, OREAD);
	while((c = Bgetrune(&in)) >= 0){
		if(c == '\n')
			continue;
		for(m = 0, v = c, i = 0; i < 5; i++, v /= 10)
			if(v % 10)
				m |= 1 << i;
		if(m & 16)
			sysfatal("U+%.4luX writes the ten-thousands place", c);
		if(used[m]++ == 0)
			first[m] = c;
	}
	for(n = 0, m = 0; m < 16; m++){
		if(used[m] == 0)
			continue;
		n++;
		print("%c%c%c%c  %5ld  first U+%.4luX\n",
			m & 8 ? 'T' : '-', m & 4 ? 'H' : '-', m & 2 ? 'X' : '-', m & 1 ? 'U' : '-', used[m], first[m]);
	}
	print("zplaces: %d of the 16 vertices of the 4-cube used; ten thousands never written\n", n);
	exits(nil);
}
