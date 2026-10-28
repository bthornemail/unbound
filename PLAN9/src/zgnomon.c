/*
 * zgnomon - why I-195's gnomon has eight frames: polytron's
 * K(p, C) = rotl(p,1) ^ rotl(p,3) ^ rotr(p,2) ^ C on 16-bit words
 * (tetra-gnomon-anim.c; boxedPINE's delta16), checked over every state.
 * Its linear part L is the polynomial x + x^3 + x^14 modulo x^16 - 1 over
 * GF(2), where the square of a sum is the sum of the squares (the cross terms
 * ab + ba cancel). Squaring three times: x^2 + x^6 + x^12, x^4 + x^12 + x^8,
 * x^8 + x^8 + 1 = 1. So L^8 is the identity; and K^8(p) = p ^ S(C), where
 * S = L^0 + ... + L^7 sends every constant to 0, so every state returns
 * within 8 steps whatever C is. Which states return sooner depends on C:
 * I-195's counts, all 65,536 states under 0x1D of period 8, and under 0x1D1D
 * 8 fixed, 56 of period 2, 4,032 of period 4, 61,440 of period 8.
 */
#include <u.h>
#include <libc.h>

static int
rotl(int x, int n)
{
	return (x << n | x >> (16 - n)) & 0xffff;
}

static int
L(int p)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14);
}

static int
Lk(int p, int k)
{
	while(k-- > 0)
		p = L(p);
	return p;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

static void
periods(int c, long count[10])
{
	int p, q, k;

	memset(count, 0, 10 * sizeof count[0]);
	for(p = 0; p < 0x10000; p++){
		for(q = L(p) ^ c, k = 1; q != p && k < 9; k++)
			q = L(q) ^ c;
		count[k]++;
	}
}

void
main(void)
{
	int e[3] = { 1, 3, 14 }, sq, i, j, n, p, c, s, k, odd;
	long count[10];
	int exps[16];
	char buf[64], *b;

	/* square the exponents three times, keeping those left an odd number of times */
	for(sq = 1; sq <= 3; sq++){
		memset(exps, 0, sizeof exps);
		for(i = 0; i < 3; i++)
			exps[(e[i] << sq) % 16] ^= 1;
		b = buf;
		for(n = 0, j = 0; j < 16; j++)
			if(exps[j]){
				b = seprint(b, buf + sizeof buf, " x^%d", j);
				n++;
			}
		print("L^%d:%s\n", 1 << sq, n ? buf : " 0");
	}
	check("L^8 is the exponent 0 alone: x^8 + x^8 + 1 = 1", n == 1 && exps[0] == 1);
	for(odd = 0, p = 0; p < 0x10000; p++)
		odd += Lk(p, 8) != p;
	check(smprint("L^8 is the identity on every state (%d of 65536 otherwise)", odd), odd == 0);
	for(odd = 0, p = 0; p < 0x10000; p++)
		odd += Lk(p, 4) != p;
	check(smprint("L^4 is not (%d of 65536 states move)", odd), odd > 0);
	for(odd = 0, c = 0; c < 0x10000; c++){
		for(s = 0, k = 0; k < 8; k++)
			s ^= Lk(c, k);
		odd += s != 0;
	}
	check(smprint("L^0 + ... + L^7 sends every constant to 0 (%d of 65536 otherwise): K^8 is the identity for every C", odd), odd == 0);
	periods(0x1d, count);
	check("0x1D: all 65,536 states of period 8 (I-195)", count[8] == 65536);
	periods(0x1d1d, count);
	check(smprint("0x1D1D: %ld fixed, %ld of period 2, %ld of period 4, %ld of period 8 (I-195: 8, 56, 4032, 61440)",
		count[1], count[2], count[4], count[8]),
		count[1] == 8 && count[2] == 56 && count[4] == 4032 && count[8] == 61440);
	exits(fails ? "fail" : nil);
}
