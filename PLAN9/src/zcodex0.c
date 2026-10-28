/*
 * zcodex0 - _CODEX[0]: each line of a text in braille (one cell per UTF-8
 * byte, U+2800 + byte) and in Aegean numerals (each code point's value by
 * place, units U+10107, tens U+10110, hundreds U+10119, thousands U+10122,
 * ten thousands U+1012B), one group per code point.
 *
 *	zcodex0 <text >codex
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static Rune place[] = { 0x10107, 0x10110, 0x10119, 0x10122, 0x1012B };

void
main(void)
{
	Biobuf in, out;
	char *line, *s, buf[UTFmax];
	Rune r;
	int n, i, d[5], first;
	ulong v;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	while((line = Brdstr(&in, '\n', 1)) != nil){
		for(first = 1, s = line; *s; first = 0){
			n = chartorune(&r, s);
			runetochar(buf, &r);
			if(!first)
				Bputc(&out, ' ');
			for(i = 0; i < n; i++)
				Bputrune(&out, 0x2800 + (uchar)buf[i]);
			s += n;
		}
		Bputc(&out, '\n');
		for(first = 1, s = line; *s; first = 0){
			s += chartorune(&r, s);
			if(r > 99999)
				sysfatal("U+%uX is past ninety thousand", r);
			if(!first)
				Bputc(&out, ' ');
			for(v = r, i = 0; i < 5; i++, v /= 10)
				d[i] = v % 10;
			for(i = 4; i >= 0; i--)
				if(d[i] > 0)
					Bputrune(&out, place[i] + d[i] - 1);
		}
		Bputc(&out, '\n');
		free(line);
	}
	Bterm(&out);
	exits(nil);
}
