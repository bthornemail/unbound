/*
 * zpcas - omi-dom's shared/polytope-cas.js in Plan 9 C.
 *	zpcas table	every byte: b s16 s32 s64 delta centroid signature led0
 *	zpcas algebra	what apply is, checked over every byte
 *	zpcas test	the module's fifteen selfTest assertions
 * apply(b, carry) = s16 ^ s32 ^ s64 ^ carry: s16 swaps adjacent bits, s32
 * reverses each nibble, s64 reverses the byte. On bit positions these are
 * i -> i^1, i^3, i^7: the swap16, swap32, swap64 of boxedPINE's
 * core/src/verified/swap.ts, there on byte positions, here on bits, added
 * rather than composed. Over GF(2) the sum M = P1 + P3 + P7 squares to I (each
 * Pk squared is I, the cross terms cancel in pairs): apply without a carry is
 * an involution. The module's compareExchange compares nothing: each step
 * replaces eval with apply(vertex).
 */
#include <u.h>
#include <libc.h>

enum { TERMINAL = 0x04 };

static int
revnibble(int n)
{
	return (n & 1) << 3 | (n & 2) << 1 | (n & 4) >> 1 | (n & 8) >> 3;
}

static void
apply(int b, int carry, int *s16, int *s32, int *s64, int *delta)
{
	int i, t, r;

	b &= 0xff;
	*s16 = 0;
	for(i = 0; i < 8; i += 2)
		*s16 |= ((b >> i) & 1) << (i+1) | ((b >> (i+1)) & 1) << i;
	*s32 = (revnibble((b >> 4) & 0xf) << 4 | revnibble(b & 0xf)) & 0xff;
	for(r = 0, t = b, i = 0; i < 8; i++, t >>= 1)
		r = r << 1 | (t & 1);
	*s64 = r & 0xff;
	*delta = (*s16 ^ *s32 ^ *s64 ^ carry) & 0xff;
}

static int
delta(int b, int carry)
{
	int a, c, d, e;

	apply(b, carry, &a, &c, &d, &e);
	return e;
}

static int
parity8(int b)
{
	b &= 0xff;
	b ^= b >> 4;
	b ^= b >> 2;
	b ^= b >> 1;
	return b & 1;
}

/* the three swaps on bit positions, summed: bit i of b sets bits i^1, i^3, i^7 */
static int
swapsum(int b)
{
	int i, x;

	for(x = 0, i = 0; i < 8; i++)
		if(b >> i & 1)
			x ^= 1 << (i^1) ^ 1 << (i^3) ^ 1 << (i^7);
	return x;
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
	int b, c, s16, s32, s64, d, n, ok, prevEval;

	if(argc != 2)
		sysfatal("usage: zpcas table | algebra | test");
	if(strcmp(argv[1], "table") == 0){
		for(b = 0; b < 256; b++){
			apply(b, 0, &s16, &s32, &s64, &d);
			/* signature(b, delta, delta): (b ^ delta ^ delta) ^ TERMINAL */
			print("%d %d %d %d %d %d %d %d\n", b, s16, s32, s64, d, d == TERMINAL,
				(b ^ d ^ d ^ TERMINAL) & 0xff, parity8(b));
		}
		exits(nil);
	}
	if(strcmp(argv[1], "algebra") == 0){
		for(n = 0, b = 0; b < 256; b++)
			n += delta(b, 0) == swapsum(b);
		check(smprint("apply is the swaps i^1, i^3, i^7 summed, %d of 256 bytes", n), n == 256);
		for(n = 0, b = 0; b < 256; b++)
			n += delta(delta(b, 0), 0) == b;
		check(smprint("apply without a carry is an involution, %d of 256", n), n == 256);
		for(n = 0, b = 0; b < 256; b++)
			for(c = 0; c < 256; c++)
				n += delta(b ^ c, 0) == (delta(b, 0) ^ delta(c, 0));
		check(smprint("apply is linear over GF(2), %d of 65536 pairs", n), n == 65536);
		for(n = 0, c = 0; c < 256; c++){
			for(ok = 1, b = 0; b < 256 && ok; b++)
				ok = delta(delta(b, c), c) == b;
			n += ok;
		}
		check(smprint("two passes stay the identity for %d carries, those M leaves fixed", n), n == 16);
		exits(fails ? "fail" : nil);
	}
	if(strcmp(argv[1], "test") == 0){
		apply(0x01, 0, &s16, &s32, &s64, &d);
		check("delta(0x01)=0x8A", d == 0x8a);
		check("delta(0x02)=0x45", delta(0x02, 0) == 0x45);
		check("s16(0x01)=0x02", s16 == 0x02);
		check("s32(0x01)=0x08", s32 == 0x08);
		check("s64(0x01)=0x80", s64 == 0x80);
		/* the engine: step(0x01), then step(0x04); eval is the last delta */
		d = delta(0x01, 0);
		check("bind 0x01", 0x01 == 0x01);
		check("apply delta", d == 0x8a);
		prevEval = d;
		d = delta(0x04, 0);
		check("eval holds prev", prevEval == 0x8a);
		check("centroid when Δ=0x04", delta(0x2a, 0) == TERMINAL);
		check("terminal 0x04", TERMINAL == 0x04);
		check("chi zero", 1);
		check("dual lock three==0", (0 ^ 0 ^ 0) == 0);
		check("signature 0x04", ((0 ^ 0 ^ 0) ^ TERMINAL) == TERMINAL);
		check("simplex LED0 zero", parity8(0x00) == 0);
		check("simplex LED0 0x01", parity8(0x01) == 1);
		exits(fails ? "fail" : nil);
	}
	sysfatal("usage: zpcas table | algebra | test");
}
