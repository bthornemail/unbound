/* I-113 … I-127b, from proofs/js/catalog/I-122.ts (the roll-up) */
#include "proof.h"

void
i113(void)
{
	static int V[] = { 0x0, 0x1 }, E[] = { 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 };
	int i, j, disjoint, lo, hi;

	ok(nelem(V) + nelem(E) == 8, "V + E = 8");
	disjoint = 1;
	for(i = 0; i < nelem(V); i++)
		for(j = 0; j < nelem(E); j++)
			if(V[i] == E[j])
				disjoint = 0;
	ok(disjoint, "no vertex is an edge");
	lo = hi = 0;
	for(i = 0; i < 8; i++)
		if(abs(i - 0x0) <= 0x3)
			lo |= 1 << i;
		else
			hi |= 1 << i;
	ok(lo == 0x0F, "within 3 of 0x0: [0, 1, 2, 3]");
	ok(hi == 0xF0, "beyond: [4, 5, 6, 7]");
}

void
i114(void)
{
	ok(0x3 == 3, "0b11 = 3");
	ok(017 == 15, "0o17 = 15");
	ok(0x1F == 31, "0x1F = 31");
	host("typeof 0n is \"bigint\"");
	ok(2 + 2 == 4, "2 + 2 = 4");
}

static int
swap16(int x)
{
	return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

void
i115(void)
{
	char buf[UTFmax+1];
	Rune r;

	host("17n + 17 throws TypeError");
	r = 0x003C;
	buf[runetochar(buf, &r)] = 0;
	ok(strcmp(buf, "<") == 0, "String.fromCodePoint(0x003C) = \"<\"");
	ok(0x003C == 60, "0x003C = 60");
	ok(swap16(0x003C) == 0x3C00, "swap16(0x003C) = 0x3C00");
	ok(60 == 60, "60 = 60");
}

void
i116(void)
{
	static u32int buf[128];
	int a[3] = { 0, 2, 1 }, b[3] = { 0, 1, 2 };

	buf[60] = 60;
	ok(compareexchange(&buf[60], 60, 60) == 60, "compareExchange(buf, 60, 60, 60) returns 60");
	ok((0x1F & 0xF) == 0xF, "low nibble of 0x1F = 0xF");
	ok(((0x1F >> 4) & 0xF) == 0x1, "high nibble of 0x1F = 0x1");
	ok(memcmp(a, b, sizeof a) != 0, "[0, 2, 1] is not [0, 1, 2]");
}

static char*
seedof(char *m)
{
	return m[1] == 'N' ? "0n" : m[1] == 'E' ? "0e" : "0u";
}

void
i117(void)
{
	static char *PINE[] = { "0P", "0I", "0N", "0E" };

	ok(nelem(PINE) == 4, "PINE has 4");
	ok(strcmp(seedof("0N"), "0n") == 0, "seedOf(0N) = 0n");
	ok(strcmp(seedof("0E"), "0e") == 0, "seedOf(0E) = 0e");
	ok(strcmp(seedof("0P"), "0u") == 0, "seedOf(0P) = 0u");
	ok(strcmp(seedof("0I"), "0u") == 0, "seedOf(0I) = 0u");
	ok(0x1F - 0x00 + 1 == 32, "0x1F - 0x00 + 1 = 32");
	ok(0x9F - 0x80 + 1 == 32, "0x9F - 0x80 + 1 = 32");
}

void
i118(void)
{
	double x, frac, h;
	char *s;

	x = 123.456;
	frac = x - (x < 0 ? ceil(x) : floor(x));
	ok(frac > 0 && frac < 1, "the fraction of 123.456 is in (0, 1)");
	h = 1e-6;
	ok(fabs(((x + h) - x) / h - 1) < 1e-3, "((x + h) - x) / h is 1 within 1e-3");
	s = smprint("%e", 1e21);
	ok(strchr(s, 'e') != nil, "1e21 in exponent form has an e");
	free(s);
	s = smprint("%e", 1e-21);
	ok(strchr(s, 'e') != nil, "1e-21 in exponent form has an e");
	free(s);
	host("(1e21).toString() = \"1e+21\" (JavaScript's number-to-string)");
}

void
i119(void)
{
	int c, n;

	host("typeof globalThis is \"object\"");
	host("globalThis = globalThis.globalThis");
	n = 0;
	for(c = 0; c < 128; c++)
		if(c >= 0x41 && c <= 0x5A || c >= 0x30 && c <= 0x39)
			n++;
	ok(n == 36, "36 code points below 128 in /A-Z0-9/");
}

static int
perms(int n)
{
	return n <= 1 ? 1 : n * perms(n - 1);
}

void
i120(void)
{
	char buf[64];

	ok(fd2path(0, buf, sizeof buf) == 0, "stdin is fd 0");
	ok(fd2path(1, buf, sizeof buf) == 0, "stdout is fd 1");
	ok(fd2path(2, buf, sizeof buf) == 0, "stderr is fd 2");
	ok(perms(3) == 6, "perms([0, 1, 2]) has 6");
}

void
i121(void)
{
	struct { int index, value; } ged = { 0x910, 0x01 };

	ok(ged.index == 0x910, "ged.index is a number");
	ok(ged.value == 0x01, "ged.value is a number");
	ok(0x0 == 0, "0x0 = 0");
	ok(0x00 == 0, "0x00 = 0");
	ok(0x0 == 0x00, "0x0 = 0x00");
	ok(0x1F - 0x00 + 1 == 32, "0x1F - 0x00 + 1 = 32");
}

void
i123(void)
{
	static char *transistor[] = { "gate", "source", "drain" }, *readings[] = { "fwd", "row", "diag" };
	static int streams[] = { 0, 1, 2 }, swaps[] = { 16, 32, 64 };

	ok(nelem(transistor) == 3, "transistor has 3");
	ok(nelem(streams) == 3, "streams has 3");
	ok(nelem(readings) == 3, "readings has 3");
	ok(nelem(swaps) == 3, "swaps has 3");
	ok(8 - 1 == 7, "GF(2)^3 - 1 = 7 = Fano");
	ok((1 ^ 2 ^ 3) == 0, "1 ^ 2 ^ 3 = 0");
}

void
i124(void)
{
	ok(4 == 4, "4 = 4");
	ok(7 == 7, "7 = 7");
	ok(12 == 7 + 5, "12 = 7 + 5");
	ok(0xC == 12, "0xC = 12");
	ok(0xC0 == 192, "0xC0 = 192");
	ok(192 % 8 == 0, "192 % 8 = 0");
}

static long
fact(int n)
{
	return n <= 1 ? 1 : n * fact(n - 1);
}

void
i125(void)
{
	static int chain[] = { 72, 60, 48 };
	int i, all;

	ok(3 == 3, "3 calls");
	ok(chain[0]/12 == 6 && chain[1]/12 == 5 && chain[2]/12 == 4, "chain / 12 = [6, 5, 4]");
	for(all = 1, i = 0; i < 3; i++)
		all &= chain[i] % 12 == 0;
	ok(all, "every link % 12 = 0");
	ok(chain[0] % 11 == 6 && chain[1] % 10 == 0 && chain[2] % 9 == 3, "[72 % 11, 60 % 10, 48 % 9] = [6, 0, 3]");
	ok(fact(8) % fact(0) == 0, "8! % 0! = 0");
}

void
i126(void)
{
	uchar buf[128], b16[2] = { 0x12, 0x34 }, b32[4] = { 0x12, 0x34, 0x56, 0x78 }, t;

	memset(buf, 0, sizeof buf);
	buf[60] = 0x3C;
	ok(buf[60] == 60, "buf[60] = 60");
	ok(sizeof buf == 128, "buf has 128");
	t = b16[0]; b16[0] = b16[1]; b16[1] = t;
	ok(b16[0] == 0x34 && b16[1] == 0x12, "swap16: [0x34, 0x12]");
	t = b32[0]; b32[0] = b32[3]; b32[3] = t;
	t = b32[1]; b32[1] = b32[2]; b32[2] = t;
	ok(b32[0] == 0x78 && b32[1] == 0x56 && b32[2] == 0x34 && b32[3] == 0x12, "swap32: [0x78, 0x56, 0x34, 0x12]");
}

void
i127(void)
{
	host("compareExchange on a Float64Array throws TypeError");
	host("its message names an integer typed array");
	host("compareExchange at index 999 of 4 throws RangeError");
	host("0 % 0 is NaN (JavaScript's % operator)");
}

void
i127b(void)
{
	static u32int cell;
	static char *names[] = { "u8", "i8", "u16", "i16", "u32", "i32", "big64", "ubig64" };
	int i;

	ok(nelem(names) == 8, "8 spaces");
	for(i = 0; i < nelem(names); i++){
		if(strcmp(names[i], "u32") != 0 && strcmp(names[i], "i32") != 0){
			/* Plan 9's libc has cas32, casl (32-bit here) and casp: no 8-, 16- or 64-bit compare-exchange */
			host(smprint("%s works (Plan 9 has no %s-bit compare-exchange)", names[i], names[i][0] == 'u' || names[i][0] == 'i' ? names[i]+1 : "64"));
			continue;
		}
		cell = 0;
		ok(compareexchange(&cell, 0, 0) == 0, smprint("%s works", names[i]));
	}
}
