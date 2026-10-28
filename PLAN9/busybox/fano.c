/*
 * fano - every 7-bit state resolves to one of the 16 Fano constants in at
 * most two steps (boxedPINE proofs/busybox/catalog/B-FANO.sh, fano.ts).
 */
#include <u.h>
#include <libc.h>

static int L[7][3] = { {1,2,3}, {1,4,5}, {1,6,7}, {2,4,6}, {2,5,7}, {3,4,7}, {3,5,6} };

static int bit(int m, int n) { return (m >> (n - 1)) & 1; }

static int
fstep(int m)
{
	int o, i, a, b, c;

	m &= 127;
	o = m;
	for(i = 0; i < 7; i++){
		a = bit(m, L[i][0]); b = bit(m, L[i][1]); c = bit(m, L[i][2]);
		o |= (a & b) << (L[i][2] - 1) | (a & c) << (L[i][1] - 1) | (b & c) << (L[i][0] - 1);
	}
	return o;
}

static int
fperp(int m)
{
	int out, x, n, even, p;

	m &= 127;
	out = 0;
	for(x = 1; x < 8; x++){
		even = 1;
		for(n = 1; n < 8; n++)
			if(bit(m, n)){
				p = x & n;
				p ^= p >> 2;
				p ^= p >> 1;
				if(p & 1)
					even = 0;
			}
		if(even)
			out |= 1 << (x - 1);
	}
	return out;
}

static void
fail(char *s)
{
	fprint(2, "FAIL(FANO): %s\n", s);
	exits("fail");
}

void
main(void)
{
	int s, s1, s2, s3, nfix, i, T[16];

	/* assert: every state converges in <= 2 steps to a fixed mask that is a constant */
	for(s = 0; s < 128; s++){
		s1 = fstep(s); s2 = fstep(s1); s3 = fstep(s2);
		if(s3 != s2)
			fail(smprint("state %d not fixed within two steps (%d -> %d -> %d -> %d)", s, s, s1, s2, s3));
		if(fstep(s2) != s2)
			fail(smprint("state %d converged to %d, not a constant", s, s2));
	}
	/* assert: exactly 16 fixed points */
	for(nfix = 0, s = 0; s < 128; s++)
		if(fstep(s) == s)
			nfix++;
	if(nfix != 16)
		fail(smprint("expected 16 fixed points, found %d", nfix));
	/* the ordered table T0..T15, as fano.ts */
	for(i = 0; i < 8; i++)
		T[i] = i == 0 ? 0 : 1 << (i - 1);
	for(i = 8; i <= 14; i++)
		T[i] = fperp(1 << (14 - i));
	T[15] = 127;
	/* assert: all sixteen table entries are constants */
	for(i = 0; i < 16; i++)
		if(fstep(T[i]) != T[i])
			fail(smprint("table[%d]=%d is not a constant", i, T[i]));
	/* assert: the dual law perp(CONSTANTS[i]) == CONSTANTS[i XOR 15] */
	for(i = 0; i < 16; i++)
		if(fperp(T[i]) != T[i ^ 15])
			fail(smprint("dual law fails at i=%d: perp(%d)=%d, table[%d]=%d", i, T[i], fperp(T[i]), i ^ 15, T[i ^ 15]));
	print("FANO: 128 states converge in <=2 steps to the 16 constants; dual is i XOR 15 - passed\n");
	exits(nil);
}
