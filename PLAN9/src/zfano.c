/*
 * zfano - omi-dom's shared/fano-7d-gate.js in Plan 9 C.
 *	zfano table	every byte: b phase parity once twice closed half full admit line
 *	zfano axioms	the Fano plane's axioms over FANO_LINES, and the masks' XOR
 *	zfano test	the module's sixteen selfTest assertions
 * phase = popcount mod 7 (0 read as 7); parity = (b ^ mask) >> 5; the 13-mask
 * closure XORs b through MASKS_13 once and twice; half and full are the
 * chirality locks (popcount 4; popcount 0 or 8); admit is gateByte with
 * requireIncidence, requireClosure and chirality 30; line = (phase - 1) mod 7.
 * The token forms are the module's own regular expressions, under regexp(2).
 */
#include <u.h>
#include <libc.h>
#include <regexp.h>

static uchar masks13[] = { 0x00, 0x07, 0xff, 0x78, 0x87, 0x20, 0x80, 0xaa, 0x55, 0x27, 0xd8, 0xa0, 0x07 };
static uchar buf3[] = { 0x00, 0x07, 0xff };	/* the selfTest's gateBuffer input */
static int lines[7][3] = { {0,1,2}, {0,3,4}, {0,5,6}, {1,3,5}, {1,4,6}, {2,3,6}, {2,4,5} };

static int
popcount8(int v)
{
	int n;

	for(n = 0, v &= 0xff; v; v &= v - 1)
		n++;
	return n;
}

static int
phase(int b)
{
	int p;

	p = popcount8(b) % 7;
	return p == 0 ? 7 : p;
}

static int
parity(int b, int mask)
{
	return ((b ^ mask) >> 5) & 7;
}

/* the cascade once and twice */
static void
closure(int b, int *once, int *twice)
{
	int i, x;

	x = b & 0xff;
	for(i = 0; i < nelem(masks13); i++)
		x ^= masks13[i];
	*once = x;
	for(i = 0; i < nelem(masks13); i++)
		x ^= masks13[i];
	*twice = x;
}

static int
chirality(int b, int mode)
{
	int pc;

	pc = popcount8(b);
	if(mode == 30)
		return pc == 4;
	if(mode == 120)
		return pc == 0 || pc == 8;
	return 0;
}

static int
onLine(int point, int line)
{
	int i;

	for(i = 0; i < 3; i++)
		if(lines[line % 7][i] == point % 7)
			return 1;
	return 0;
}

/* gateByte(b, { requireIncidence, requireClosure, chirality: 30 }) */
static int
admit(int b)
{
	int once, twice;

	closure(b, &once, &twice);
	return parity(b, 0) != 0 && twice == b && chirality(b, 30);
}

static int
match(char *re, char *s)
{
	Reprog *p;
	int m;

	if((p = regcomp(re)) == nil)
		sysfatal("regcomp %s", re);
	m = regexec(p, s, nil, 0);
	free(p);
	return m;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

void
main(int argc, char *argv[])
{
	int b, once, twice, i, j, k, l, n, x, fold;

	if(argc != 2)
		sysfatal("usage: zfano table | axioms | test");
	if(strcmp(argv[1], "table") == 0){
		for(b = 0; b < 256; b++){
			closure(b, &once, &twice);
			print("%d %d %d %d %d %d %d %d %d %d\n", b, phase(b), parity(b, 0), once, twice,
				twice == b, chirality(b, 30), chirality(b, 120), admit(b), (phase(b) - 1) % 7);
		}
		exits(nil);
	}
	if(strcmp(argv[1], "axioms") == 0){
		for(x = 0, i = 0; i < nelem(masks13); i++)
			x ^= masks13[i];
		check(smprint("the 13 masks XOR to %#.2ux, so one pass is already the identity", x), x == 0);
		for(i = 0; i < 7; i++)
			for(j = i+1; j < 7; j++){
				for(n = 0, l = 0; l < 7; l++)
					n += onLine(i, l) && onLine(j, l);
				if(n != 1)
					check(smprint("points %d %d share %d lines", i, j, n), 0);
			}
		check("every pair of the 7 points lies on exactly one line", fails == 0);
		for(i = 0; i < 7; i++){
			for(n = 0, l = 0; l < 7; l++)
				n += onLine(i, l);
			if(n != 3)
				check(smprint("point %d is on %d lines", i, n), 0);
		}
		check("every point lies on 3 lines", fails == 0);
		exits(fails ? "fail" : nil);
	}
	if(strcmp(argv[1], "test") == 0){
		check("7 lines", nelem(lines) == 7);
		check("each line 3 points", nelem(lines[0]) == 3);
		check("13 masks", nelem(masks13) == 13);
		check("phase zero → 7", phase(0x00) == 7);
		check("phase 0x07 → 3", phase(0x07) == 3);
		check("parity 0x07 → 0", parity(0x07, 0) == 0);
		check("parity 0x20 → 1", parity(0x20, 0) == 1);
		check("incidence 1", (1 & 7) != 0);
		check("no incidence 0", (0 & 7) == 0);
		closure(0x5a, &once, &twice);
		check("13-mask involution", twice == 0x5a);
		check("point 0 on line 0", onLine(0, 0));
		check("point 6 on line 2", onLine(6, 2));
		check("tick regex", match("^(1|2|3|4|5|6|7)?:([0-6])?:([0-6])?$", "7:3:1"));
		check("incidence regex", match("^(0x0[1-7]|[1-7])$", "0x05"));
		/* gateByte(0x0f, { chirality: 30 }): strict, and the only reason could be chirality */
		check("chirality half on 0x0f", chirality(0x0f, 30) && popcount8(0x0f) == 4);
		for(fold = 0, k = 0; k < nelem(buf3); k++)
			fold ^= buf3[k];
		closure(fold, &once, &twice);
		check("buffer fold closed", twice == fold);
		exits(fails ? "fail" : nil);
	}
	sysfatal("usage: zfano table | axioms | test");
}
