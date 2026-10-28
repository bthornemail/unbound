/*
 * zrender - one term, rendered by codex index, from the witnesses that hold it.
 *
 *	zrender NAME figure zeroth outdir
 *
 * figure: CODEX<tab>reference<tab>literal, one line per witness that holds
 * the term. zeroth: one zeroth per line (YHWH, YAHWEH, GOD IS WORD, ...).
 * Writes three files to outdir:
 *	codices.wsn	_CODICES: each codex index, its literal and reference, and
 *			its bytes XOR and XNOR each zeroth's, one braille cell a byte
 *	codex		_CODEX[0]: each codex index in braille (one cell per UTF-8
 *			byte, U+2800 + byte) and in Aegean numerals (each code point's
 *			value by place)
 *	codex.wsn	_CODEX[1]: the same, in Wirth syntax notation
 * A codex that lacks the term is optional whitespace, OPTWHITESPACE = { " " } .
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static char *CODEX[] = { "ALEPPO", "ROSETTA", "BEHISTUN", "LENINGRAD" };
static Rune place[] = { 0x10107, 0x10110, 0x10119, 0x10122, 0x1012B };

typedef struct Wit Wit;
struct Wit {
	char	*ref;
	char	*lit;
};
static Wit wit[nelem(CODEX)];
static int held[nelem(CODEX)];
static char *zero[16];
static int nzero;

/* identifier: the letters, upper case */
static char*
id(char *s)
{
	char *t, *p;

	t = p = malloc(strlen(s) + 1);
	for(; *s; s++)
		if(*s >= 'a' && *s <= 'z')
			*p++ = *s - 'a' + 'A';
		else if(*s >= 'A' && *s <= 'Z')
			*p++ = *s;
	*p = 0;
	return t;
}

/* a WSN literal: a quote inside is written twice */
static void
lit(Biobuf *b, char *s)
{
	Bputc(b, '"');
	for(; *s; s++){
		if(*s == '"')
			Bputc(b, '"');
		Bputc(b, *s);
	}
	Bputc(b, '"');
}

static char*
cells(char *a, char *z, int xnor)
{
	Fmt f;
	int na, nz, n, i;
	uchar x;

	na = strlen(a); nz = strlen(z);
	n = na > nz ? na : nz;
	fmtstrinit(&f);
	for(i = 0; i < n; i++){
		x = (i < na ? a[i] : 0) ^ (i < nz ? z[i] : 0);
		if(xnor)
			x ^= 0xFF;
		fmtprint(&f, "%C", (Rune)(0x2800 + x));
	}
	return fmtstrflush(&f);
}

static char*
braille(char *s)
{
	Fmt f;
	Rune r;
	char buf[UTFmax];
	int n, i, first;

	fmtstrinit(&f);
	for(first = 1; *s; first = 0){
		n = chartorune(&r, s);
		runetochar(buf, &r);
		if(!first)
			fmtprint(&f, " ");
		for(i = 0; i < n; i++)
			fmtprint(&f, "%C", (Rune)(0x2800 + (uchar)buf[i]));
		s += n;
	}
	return fmtstrflush(&f);
}

static char*
aegean(char *s)
{
	Fmt f;
	Rune r;
	ulong v;
	int i, d[5], first;

	fmtstrinit(&f);
	for(first = 1; *s; first = 0){
		s += chartorune(&r, s);
		if(r > 99999)
			sysfatal("U+%uX is past ninety thousand", r);
		if(!first)
			fmtprint(&f, " ");
		for(v = r, i = 0; i < 5; i++, v /= 10)
			d[i] = v % 10;
		for(i = 4; i >= 0; i--)
			if(d[i] > 0)
				fmtprint(&f, "%C", place[i] + d[i] - 1);
	}
	return fmtstrflush(&f);
}

static Biobuf*
create1(char *dir, char *name)
{
	Biobuf *b;
	char *p;

	p = smprint("%s/%s", dir, name);
	if((b = Bopen(p, OWRITE)) == nil)
		sysfatal("create %s: %r", p);
	free(p);
	return b;
}

/* the top production: each codex index in order, absent ones as whitespace */
static void
top(Biobuf *b, char *name)
{
	int j, ws;

	Bprint(b, "%s =", id(name));
	for(ws = 0, j = 0; j < nelem(CODEX); j++){
		Bprint(b, " %s", held[j] ? CODEX[j] : "OPTWHITESPACE");
		ws |= !held[j];
	}
	Bprint(b, " .\n");
	if(ws)
		Bprint(b, "OPTWHITESPACE = { \" \" } .\n");
}

void
main(int argc, char *argv[])
{
	Biobuf *in, *ci, *cx, *cw;
	char *line, *f[3], *c;
	int j, k;

	if(argc != 5){
		fprint(2, "usage: zrender NAME figure zeroth outdir\n");
		exits("usage");
	}
	if((in = Bopen(argv[2], OREAD)) == nil)
		sysfatal("open %s: %r", argv[2]);
	while((line = Brdstr(in, '\n', 1)) != nil){
		if(getfields(line, f, 3, 0, "\t") != 3)
			continue;
		for(j = 0; j < nelem(CODEX); j++)
			if(strcmp(f[0], CODEX[j]) == 0){
				held[j] = 1;
				wit[j].ref = f[1];
				wit[j].lit = f[2];
			}
	}
	Bterm(in);
	if((in = Bopen(argv[3], OREAD)) == nil)
		sysfatal("open %s: %r", argv[3]);
	while(nzero < nelem(zero) && (line = Brdstr(in, '\n', 1)) != nil)
		zero[nzero++] = line;
	Bterm(in);

	ci = create1(argv[4], "codices.wsn");
	cx = create1(argv[4], "codex");
	cw = create1(argv[4], "codex.wsn");
	top(ci, argv[1]);
	top(cw, argv[1]);
	for(j = 0; j < nelem(CODEX); j++){
		Bprint(cx, "[%d] %s\n", j, CODEX[j]);
		if(!held[j]){
			Bprint(cx, "\n");
			continue;
		}
		c = CODEX[j];
		/* _CODICES */
		Bprint(ci, "%s = %sLITERAL %sAT", c, c, c);
		for(k = 0; k < nzero; k++)
			Bprint(ci, " %sXOR%s %sXNOR%s", c, id(zero[k]), c, id(zero[k]));
		Bprint(ci, " .\n");
		Bprint(ci, "%sLITERAL = ", c); lit(ci, wit[j].lit); Bprint(ci, " .\n");
		Bprint(ci, "%sAT = ", c); lit(ci, wit[j].ref); Bprint(ci, " .\n");
		for(k = 0; k < nzero; k++){
			Bprint(ci, "%sXOR%s = ", c, id(zero[k])); lit(ci, cells(wit[j].lit, zero[k], 0)); Bprint(ci, " .\n");
			Bprint(ci, "%sXNOR%s = ", c, id(zero[k])); lit(ci, cells(wit[j].lit, zero[k], 1)); Bprint(ci, " .\n");
		}
		/* _CODEX[0]: the rendering itself */
		Bprint(cx, "%s\n%s\n", braille(wit[j].lit), aegean(wit[j].lit));
		/* _CODEX[1]: the same, in WSN */
		Bprint(cw, "%s = %sBRAILLE %sAEGEAN .\n", c, c, c);
		Bprint(cw, "%sBRAILLE = ", c); lit(cw, braille(wit[j].lit)); Bprint(cw, " .\n");
		Bprint(cw, "%sAEGEAN = ", c); lit(cw, aegean(wit[j].lit)); Bprint(cw, " .\n");
	}
	Bterm(ci); Bterm(cx); Bterm(cw);
	exits(nil);
}
