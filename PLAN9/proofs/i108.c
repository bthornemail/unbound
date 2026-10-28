/* I-108 … I-112, from proofs/js/catalog/I-122.ts (the roll-up) */
#include "proof.h"

static int
mod(int a, int m)
{
	return ((a % m) + m) % m;
}

void
i108(void)
{
	int atoms[4], i, x, s;
	static int heads[] = { 8, 9, 10, 11 };

	for(i = 0; i < 4; i++)
		atoms[i] = mod(heads[i], 8);
	ok(atoms[0] == 0 && atoms[1] == 1 && atoms[2] == 2 && atoms[3] == 3, "atoms = [0, 1, 2, 3]");
	for(x = 0, s = 0, i = 0; i < 4; i++){
		x ^= atoms[i];
		s += atoms[i];
	}
	ok(x == 0, "xor(...atoms) = 0");
	ok(s == 6, "atoms summed = 6");
	ok((1 ^ 1 ^ 1 ^ 1) == 0, "xor(1, 1, 1, 1) = 0");
	ok(3960 == 8 * 9 * 5 * 11, "3960 = 8 * 9 * 5 * 11");
}

void
i109(void)
{
	int i, x;

	for(x = 0, i = 0; i < 8; i++)
		x ^= i;
	ok(x == 0, "xor(0..7) = 0");
	ok((1 ^ 2 ^ 3) == 0, "xor(1, 2, 3) = 0");
	ok(10 % 10 == 0, "10 % 10 = 0");
	ok(9 % 9 == 0, "9 % 9 = 0");
	ok(11 % 11 == 0, "11 % 11 = 0");
}

void
i110(void)
{
	double one, zero;

	one = 1;
	zero = 0;
	host("new Uint8Array(-1) throws RangeError");
	/* JavaScript's % is NaN for a zero divisor; Plan 9's fmod is not: */
	if(!isNaN(fmod(one, zero)))
		print("I-110 note: Plan 9 fmod(1, 0) = %g, where JavaScript's 1 %% 0 is NaN\n", fmod(one, zero));
	host("1 % 0 is NaN (JavaScript's % operator; Plan 9 C has fmod, which differs at a zero divisor)");
	ok(isInf(one / zero, 1), "1 / 0 = Infinity");
	host("17n + 17 throws TypeError (BigInt mixed with Number)");
	ok(17LL + 17LL == 34LL, "17n + 17n = 34n");
	ok(0x17LL == 23LL, "0x17n = 23n");
}

void
i111(void)
{
	static int V[] = { 0x0, 0x1 }, E[] = { 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 };
	int seen, i, r, c, faces;

	ok(nelem(V) == 2, "V has 2");
	ok(nelem(E) == 6, "E has 6");
	seen = 0;
	for(i = 0; i < nelem(V); i++)
		seen |= 1 << V[i];
	for(i = 0; i < nelem(E); i++)
		seen |= 1 << E[i];
	ok(seen == 0xFF && nelem(V) + nelem(E) == 8, "V and E together, sorted, are the nibble 0..7");
	faces = 0;
	for(r = 0; r < 8; r++)
		for(c = 0; c < 8; c++)
			faces++;
	ok(faces == 64, "64 faces (r, c)");
}

static int
swap16(int x)
{
	return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

void
i112(void)
{
	int V, E, F;

	V = 4; E = 6; F = 4;
	ok(V - E + F == 2, "V - E + F = 2");
	ok((0x8000 ^ 0x0000) == 0x8000, "0x8000 ^ 0x0000 = 0x8000");
	ok(swap16(0x1234) == 0x3412, "swap16(0x1234) = 0x3412");
	ok(3 * 2 == 6, "3 * 2 = 6");
}
