/*
 * zrunes - a check, not a port: Plan 9's rune routines (libc's chartorune, runetochar, runelen, fullrune,
 * utfnlen; here Rune 32 bits, Runemax 0x10FFFF, UTFmax 4) against UTF-8 read another way, node's TextEncoder and
 * TextDecoder (in/runes, written by in/runes/gen.mjs with the prediction of chartorune from its source filed first).
 *	zrunes encode			every Unicode scalar value in order, by runetochar
 *	zrunes decode file		the file by chartorune: every scalar value in order, and utfnlen's count (which
 *					stops at a NUL even within its length)
 *	zrunes odd			runetochar and runelen on surrogates and runes past Runemax: value length hex
 *	zrunes fullrune			fullrune on every first byte and length 0 to 4: byte n answer
 *	zrunes seqs			the 143,768 strings of gen.mjs, each followed by "A", read by chartorune
 *	zrunes -n seqs			the falsifier, and the second reading: the WHATWG decoder, a maximal prefix
 *					that cannot be completed replaced once, as node's TextDecoder reads
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static int whatwg;
static uchar S[] = { 0x00, 0x41, 0x7f, 0x80, 0x8f, 0x90, 0x9f, 0xa0, 0xbf, 0xc0, 0xc2, 0xdf, 0xe0, 0xed, 0xef, 0xf0, 0xf4, 0xf5, 0xff };
static Biobuf out;

static void
encode(void)
{
	Rune r;
	char b[UTFmax];

	for(r = 0; r <= Runemax; r++)
		if(r < 0xD800 || r > 0xDFFF)
			Bwrite(&out, b, runetochar(b, &r));
}

static void
decode(char *f)
{
	int fd, n;
	long len, cap, i, k;
	char *s;
	Rune r, want;

	if((fd = open(f, OREAD)) < 0)
		sysfatal("open %s: %r", f);
	cap = 1 << 23;
	s = malloc(cap + UTFmax + 1);
	len = 0;
	while((n = read(fd, s + len, cap - len)) > 0)
		len += n;
	close(fd);
	memset(s + len, 0, UTFmax + 1);
	want = 0;
	k = 0;
	for(i = 0; i < len; i += n){
		n = chartorune(&r, s + i);
		if(r != want){
			Bprint(&out, "at byte %ld: %ux where %ux\n", i, r, want);
			return;
		}
		k++;
		want++;
		if(want == 0xD800)
			want = 0xE000;
	}
	/* utfnlen stops at a NUL even within its length, as a string's end: U+0000 is the first byte, so count after it */
	Bprint(&out, "decode %ld runes, every scalar value in order, the last %ux; utfnlen %d at U+0000, %d after it\n", k, want - 1, utfnlen(s, len), utfnlen(s + 1, len - 1));
}

static void
odd(void)
{
	static ulong past[] = { 0x110000, 0x110001, 0x1FFFFF, 0x200000, 0x7FFFFFFF, 0xFFFFFFFF };
	ulong c;
	Rune r;
	char b[UTFmax];
	int i, n, k;

	for(c = 0xD800; c <= 0xDFFF + nelem(past); c++){
		r = c <= 0xDFFF ? c : past[c - 0xE000];
		n = runetochar(b, &r);
		if(runelen(r) != n)
			Bprint(&out, "runelen %ux %d where runetochar %d\n", r, runelen(r), n);
		Bprint(&out, "%ux %d ", r, n);
		for(k = 0; k < n; k++)
			Bprint(&out, "%.2ux", (uchar)b[k]);
		Bprint(&out, "\n");
	}
	USED(i);
}

static void
full(void)
{
	char b[UTFmax + 1];
	int c, n;

	for(c = 0; c < 256; c++)
		for(n = 0; n <= 4; n++){
			memset(b, 0x80, sizeof b);
			b[0] = c;
			Bprint(&out, "%ux %d %d\n", c, n, fullrune(b, n));
		}
}

static void
emit(Rune *v, int *nv, Rune r)
{
	v[(*nv)++] = r;
}

static int
readwhatwg(uchar *b, int n, Rune *v)	/* the WHATWG UTF-8 decoder */
{
	int i, need, seen, nv;
	Rune cp;
	uchar lo, hi, c;

	nv = need = seen = 0;
	cp = 0;
	lo = 0x80;
	hi = 0xBF;
	for(i = 0; i < n; i++){
		c = b[i];
		if(need == 0){
			if(c <= 0x7F) emit(v, &nv, c);
			else if(c >= 0xC2 && c <= 0xDF){ need = 1; cp = c & 0x1F; }
			else if(c >= 0xE0 && c <= 0xEF){ if(c == 0xE0) lo = 0xA0; if(c == 0xED) hi = 0x9F; need = 2; cp = c & 0xF; }
			else if(c >= 0xF0 && c <= 0xF4){ if(c == 0xF0) lo = 0x90; if(c == 0xF4) hi = 0x8F; need = 3; cp = c & 7; }
			else emit(v, &nv, Runeerror);
			continue;
		}
		if(c < lo || c > hi){	/* the prefix replaced once; the byte read again */
			cp = need = seen = 0;
			lo = 0x80;
			hi = 0xBF;
			emit(v, &nv, Runeerror);
			i--;
			continue;
		}
		lo = 0x80;
		hi = 0xBF;
		cp = cp << 6 | (c & 0x3F);
		if(++seen == need){
			emit(v, &nv, cp);
			cp = need = seen = 0;
		}
	}
	if(need)
		emit(v, &nv, Runeerror);
	return nv;
}

static void
one(uchar *s, int n)
{
	uchar b[8];
	Rune v[8], r;
	int i, k, nv;

	memmove(b, s, n);
	b[n] = 'A';
	b[n+1] = 0;
	if(whatwg)
		nv = readwhatwg(b, n + 1, v);
	else
		for(nv = i = 0; i < n + 1; i += k){
			k = chartorune(&r, (char*)b + i);
			v[nv++] = r;
		}
	for(i = 0; i < nv; i++)
		Bprint(&out, i ? " %ux" : "%ux", v[i]);
	Bprint(&out, "\n");
}

static void
seqs(void)
{
	uchar s[4];
	int a, b, c, d;

	for(a = 0; a < 256; a++){ s[0] = a; one(s, 1); }
	for(a = 0; a < 256; a++) for(b = 0; b < 256; b++){ s[0] = a; s[1] = b; one(s, 2); }
	for(a = 0xC0; a <= 0xFF; a++) for(b = 0; b < nelem(S); b++) for(c = 0; c < nelem(S); c++){
		s[0] = a; s[1] = S[b]; s[2] = S[c]; one(s, 3);
	}
	for(a = 0xF0; a <= 0xF7; a++) for(b = 0; b < nelem(S); b++) for(c = 0; c < nelem(S); c++) for(d = 0; d < nelem(S); d++){
		s[0] = a; s[1] = S[b]; s[2] = S[c]; s[3] = S[d]; one(s, 4);
	}
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'n':
		whatwg = 1;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zrunes [-n] encode|decode file|odd|fullrune|seqs");
	Binit(&out, 1, OWRITE);
	if(strcmp(argv[0], "encode") == 0) encode();
	else if(strcmp(argv[0], "decode") == 0 && argc > 1) decode(argv[1]);
	else if(strcmp(argv[0], "odd") == 0) odd();
	else if(strcmp(argv[0], "fullrune") == 0) full();
	else if(strcmp(argv[0], "seqs") == 0) seqs();
	else sysfatal("unknown %s", argv[0]);
	Bterm(&out);
	exits(nil);
}
