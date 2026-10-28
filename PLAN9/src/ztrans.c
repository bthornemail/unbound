/*
 * ztrans - translate text to braille and Aegean, one line per rune.
 *
 *	ztrans <text
 *
 * For each rune: its code point; its UTF-8 bytes as 8-dot braille cells
 * (U+2800 + byte); its value as Aegean numerals by place, highest first
 * (units U+10107, tens U+10110, hundreds U+10119, thousands U+10122,
 * ten thousands U+1012B, each the place's 1, plus the digit less one).
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static Rune place[] = { 0x10107, 0x10110, 0x10119, 0x10122, 0x1012B };

void
main(void)
{
	Biobuf in, out;
	Rune r;
	long c;
	char buf[UTFmax];
	int i, n, d[5];
	ulong v;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	while((c = Bgetrune(&in)) >= 0){
		r = c;
		if(r == '\n'){
			Bprint(&out, "\n");
			continue;
		}
		Bprint(&out, "U+%04uX\t", r);
		n = runetochar(buf, &r);
		for(i = 0; i < n; i++)
			Bprint(&out, "%C", (Rune)(0x2800 + (uchar)buf[i]));
		Bprint(&out, "\t");
		v = r;
		if(v > 99999)
			sysfatal("U+%uX is past ninety thousand", r);
		for(i = 0; i < 5; i++){
			d[i] = v % 10;
			v /= 10;
		}
		for(i = 4; i >= 0; i--)
			if(d[i] > 0)
				Bprint(&out, "%C", place[i] + d[i] - 1);
		Bprint(&out, "\n");
	}
	Bterm(&out);
	exits(nil);
}
