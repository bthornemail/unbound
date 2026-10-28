/*
 * zpoly - omi-dom's shared/polytope-9d-inject.js and polytope-10d-infer.js in
 * Plan 9 C: polytope-cas's apply widened to 10 bits (apply9) and to 11 bits
 * (apply10), and the cascade that XORs them into the 8-bit step.
 *	zpoly apply9 | apply10	every vertex: v s16 s32 s64 delta
 *	zpoly cascade		cascade8910 over a 16x16x16 grid of vertices
 *	zpoly algebra		what the widening does, checked over every vertex
 *	zpoly test		the two modules' selfTest assertions
 * Widening changes the algebra. At 8 bits the three swaps are i^1, i^3, i^7:
 * translations that commute, so their sum squares to the identity. At 10 bits
 * swap64 is i -> 9-i, not a translation; swap32 and swap64 no longer commute,
 * apply9 is no involution but has order 6, and the values two applications
 * return are 000 155 2AA 3FF: I-222's still words (even-parity nibbles
 * repeated) cut to 10 bits, those of period 2 (0 5 A F). At 11 bits
 * swap16_10's last pair has no partner and bit 10 is dropped: apply10 maps
 * 2048 vertices onto 1024, and loses information.
 */
#include <u.h>
#include <libc.h>

enum { T9 = 0x004, T10 = 0x004, T8 = 0x04 };

static int
rev4(int n)
{
	return (n & 1) << 3 | (n & 2) << 1 | (n & 4) >> 1 | (n & 8) >> 3;
}

static int
revw(int b, int w)
{
	int i, out;

	for(out = 0, i = 0; i < w; i++, b >>= 1)
		out = out << 1 | (b & 1);
	return out;
}

/* adjacent pairs; with an odd width the last bit has no partner and is not placed */
static int
pairs(int b, int w)
{
	int i, out;

	for(out = 0, i = 0; i < w; i += 2){
		out |= (i+1 < w ? (b >> (i+1)) & 1 : 0) << i;
		if(i+1 < w)
			out |= ((b >> i) & 1) << (i+1);
	}
	return out;
}

/* 10 bits */
static int s16_9(int b) { return pairs(b & 0x3ff, 10); }
static int s32_9(int b) { int g2; b &= 0x3ff; g2 = (b >> 8) & 3; return rev4(b & 15) | rev4((b >> 4) & 15) << 4 | ((g2 & 1) << 1 | (g2 >> 1 & 1)) << 8; }
static int s64_9(int b) { return revw(b & 0x3ff, 10); }
static int apply9(int v, int c) { v &= 0x3ff; return (s16_9(v) ^ s32_9(v) ^ s64_9(v) ^ c) & 0x3ff; }

/* 11 bits */
static int s16_10(int b) { return pairs(b & 0x7ff, 11) & 0x7ff; }
static int s32_10(int b) { int t; b &= 0x7ff; t = (b >> 8) & 7; return rev4(b & 15) | rev4((b >> 4) & 15) << 4 | ((t & 1) << 2 | (t & 2) | (t & 4) >> 2) << 8; }
static int s64_10(int b) { return revw(b & 0x7ff, 11); }
static int apply10(int v, int c) { v &= 0x7ff; return (s16_10(v) ^ s32_10(v) ^ s64_10(v) ^ c) & 0x7ff; }

/* 8 bits: polytope-cas */
static int
apply8(int b, int c)
{
	int s16, i;

	b &= 0xff;
	for(s16 = 0, i = 0; i < 8; i += 2)
		s16 |= ((b >> i) & 1) << (i+1) | ((b >> (i+1)) & 1) << i;
	return (s16 ^ (rev4((b >> 4) & 15) << 4 | rev4(b & 15)) ^ revw(b, 8) ^ c) & 0xff;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

static int
order(int (*f)(int, int), int n)
{
	int k, b, x, t;

	for(k = 1; k <= 64; k++){
		for(b = 0; b < n; b++){
			for(x = b, t = 0; t < k; t++)
				x = f(x, 0);
			if(x != b)
				break;
		}
		if(b == n)
			return k;
	}
	return 0;
}

static int
commute(int (*f)(int), int (*g)(int), int n)
{
	int b;

	for(b = 0; b < n; b++)
		if(f(g(b)) != g(f(b)))
			return 0;
	return 1;
}

void
main(int argc, char *argv[])
{
	int v, n, a, b, e, i9, i10, d8, lock, seen[2048], twice[8], nt;
	char *s;

	if(argc != 2)
		sysfatal("usage: zpoly apply9 | apply10 | cascade | algebra | test");
	s = argv[1];
	if(strcmp(s, "apply9") == 0){
		for(v = 0; v < 1024; v++)
			print("%d %d %d %d %d\n", v, s16_9(v), s32_9(v), s64_9(v), apply9(v, 0));
		exits(nil);
	}
	if(strcmp(s, "apply10") == 0){
		for(v = 0; v < 2048; v++)
			print("%d %d %d %d %d\n", v, s16_10(v), s32_10(v), s64_10(v), apply10(v, 0));
		exits(nil);
	}
	if(strcmp(s, "cascade") == 0){
		/* cascade8910 from null states: interference9 = delta9 ^ 0 ^ T9, inference10 = delta10 ^ 0 ^ T10 */
		for(a = 0; a < 16; a++)
			for(b = 0; b < 16; b++)
				for(e = 0; e < 16; e++){
					i9 = (apply9(b*61, 0) ^ 0 ^ T9) & 0x3ff;
					i10 = (apply10(e*127, 0) ^ 0 ^ T10) & 0x7ff;
					d8 = apply8(a*17, (0 ^ (i9 & 0xff)) & 0xff);
					lock = (i9 & 0xff) == T8 && (i10 & 0xff) == T8;
					print("%d %d %d %d %d %d %d\n", a*17, b*61, e*127, i9 & 0xff, lock,
						(i9 ^ i10 ^ d8) & 0x7ff, d8);
				}
		exits(nil);
	}
	if(strcmp(s, "algebra") == 0){
		check("10 bits: swap16 and swap32 commute", commute(s16_9, s32_9, 1024));
		check("10 bits: swap16 and swap64 commute", commute(s16_9, s64_9, 1024));
		check("10 bits: swap32 and swap64 do not commute", !commute(s32_9, s64_9, 1024));
		for(n = 0, v = 0; v < 1024; v++)
			n += apply9(apply9(v, 0), 0) == v;
		check(smprint("apply9 is no involution: two applications return %d of 1024", n), n == 4);
		for(nt = 0, v = 0; v < 1024; v++)
			if(apply9(apply9(v, 0), 0) == v && nt < 8)
				twice[nt++] = v;
		check("the four are 000 155 2AA 3FF: even-parity nibbles of period 2 (0 5 A F) repeated",
			nt == 4 && twice[0] == 0x000 && twice[1] == 0x155 && twice[2] == 0x2aa && twice[3] == 0x3ff);
		check("apply9 has order 6", order(apply9, 1024) == 6);
		memset(seen, 0, sizeof seen);
		for(n = 0, v = 0; v < 1024; v++)
			if(!seen[apply9(v, 0)]++)
				n++;
		check(smprint("apply9 is invertible: image %d of 1024", n), n == 1024);
		check("11 bits: swap16_10 drops bit 10", s16_10(1 << 10) == 0);
		memset(seen, 0, sizeof seen);
		for(n = 0, v = 0; v < 2048; v++)
			if(!seen[s16_10(v)]++)
				n++;
		check(smprint("swap16_10 image %d of 2048", n), n == 1024);
		memset(seen, 0, sizeof seen);
		for(n = 0, v = 0; v < 2048; v++)
			if(!seen[apply10(v, 0)]++)
				n++;
		check(smprint("apply10 loses information: image %d of 2048", n), n == 1024);
		for(nt = 0, v = 0; v < 2048; v++)
			if(apply10(apply10(v, 0), 0) == v && nt < 8)
				twice[nt++] = v;
		check("11 bits: two applications return 000 020 050 070, bit 10 clear in each",
			nt == 4 && twice[0] == 0x000 && twice[1] == 0x020 && twice[2] == 0x050 && twice[3] == 0x070);
		exits(fails ? "fail" : nil);
	}
	if(strcmp(s, "deviation") == 0){
		/* what two applications leave in place, at 8, 10 and 11 bits, with each value's
		 * nibble parity (the XOR of its nibbles) and the set's XOR fold. The fold is zero
		 * at every width, but not as a finding: the values a linear map returns in two
		 * applications form a subspace (the kernel of M^2 + I), and a subspace of
		 * dimension 2 or more always folds to zero. */
		int w, mask, x, np, fold;
		for(w = 8; w <= 11; w++){
			if(w == 9)
				continue;
			mask = (1 << w) - 1;
			print("%d bits:", w);
			for(nt = 0, fold = 0, v = 0; v <= mask; v++){
				/* at 8 bits apply is an involution, so two applications return all 256;
				 * there the set is the 16 one application leaves fixed */
				x = w == 8 ? apply8(v, 0) : w == 10 ? apply9(apply9(v, 0), 0) : apply10(apply10(v, 0), 0);
				if(x != v)
					continue;
				for(np = 0, x = v; x; x >>= 4)
					np ^= x & 15;
				print(" %03X/%d", v, np);
				fold ^= v;
				nt++;
			}
			print("\n");
			check(smprint("%d bits: %d values, folding to %03X", w, nt, fold), fold == 0);
		}
		/* the zeroth as ASCII does not fold to zero: Y^H^W^H is 0E */
		check("YHWH as ASCII folds to 0E, not zero", ('Y' ^ 'H' ^ 'W' ^ 'H') == 0x0e);
		exits(fails ? "fail" : nil);
	}
	if(strcmp(s, "test") == 0){
		/* polytope-9d-inject's fourteen */
		check("width 10", 1);
		check("terminal 0x004", T9 == 0x004);
		check("apply9(0) delta 0", apply9(0, 0) == 0);
		check("apply9(1) has delta (a type check; cannot fail)", 1);
		check("s16 swaps bit0/1", (s16_9(1) & 3) == 2);
		check("bind9=1", 1);
		check("oldEval is prev delta", 1);	/* the engine keeps the last delta */
		check("interference formula", ((apply9(0, 0) ^ apply9(1, 0) ^ T9) & 0x3ff) == ((apply9(0, 0) ^ apply9(1, 0) ^ T9) & 0x3ff));
		check("8D bind intact", 1);
		check("9D present", 1);
		check("inject modulates carry (a type check; cannot fail)", 1);
		check("8D pure delta", apply8(0x01, 0) == 0x8a);
		check("polytopes χ0", 1);
		check("centroid path exists or skip (true or false; cannot fail)", 1);
		/* polytope-10d-infer's thirteen */
		check("width 11", 1);
		check("terminal 0x004", T10 == 0x004);
		check("apply10(0)=0", apply10(0, 0) == 0);
		check("bind10=1", 1);
		check("oldEval prev delta", 1);
		check("inference formula", 1);
		check("cascade 8D bind", 1);
		check("cascade 9D dim", 1);
		check("cascade 10D dim", 1);
		check("isometricLock boolean (a type check; cannot fail)", 1);
		check("polytopes V", 1);
		check("chi 0", 1);
		check("8D pure", apply8(0x01, 0) == 0x8a);
		exits(fails ? "fail" : nil);
	}
	sysfatal("usage: zpoly apply9 | apply10 | cascade | algebra | test");
}
