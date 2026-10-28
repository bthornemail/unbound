/*
 * zdiff - the bit flips from YHWH, one line per term.
 *
 *	zdiff [-n] <text
 *
 * The first line is the base. Every line, the base included, is XORed with
 * the base's UTF-8 bytes, position by position; a byte past either end is 0.
 * Each result byte is printed as an 8-dot braille cell, U+2800 + byte, so a
 * raised dot is a flipped bit, and a blank cell is caught at the base.
 * With -n, XNOR: each cell is the complement, a raised dot a bit kept.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

void
main(int argc, char *argv[])
{
	Biobuf in, out;
	char *base, *line;
	int nb, nl, n, i;
	uchar x, nx;

	nx = argc > 1 && strcmp(argv[1], "-n") == 0 ? 0xFF : 0;
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	if((base = Brdstr(&in, '\n', 1)) == nil)
		sysfatal("no base");
	nb = strlen(base);
	line = strdup(base);
	do{
		nl = strlen(line);
		n = nb > nl ? nb : nl;
		Bprint(&out, "%s\t", line);
		for(i = 0; i < n; i++){
			x = (i < nb ? base[i] : 0) ^ (i < nl ? line[i] : 0) ^ nx;
			Bprint(&out, "%C", (Rune)(0x2800 + x));
		}
		Bprint(&out, "\n");
		free(line);
	}while((line = Brdstr(&in, '\n', 1)) != nil);
	Bterm(&out);
	exits(nil);
}
